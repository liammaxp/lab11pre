#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size): mSize(size), mX(0.f), mY(0.f), mRowHeight(0.f) {};

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) { 
        const size_t invalidID = std::numeric_limits<size_t>::max();
        if (rectSize.x <= 0.f || rectSize.y <= 0.f || rectSize.x > mSize.x || rectSize.y > mSize.y) {
            return {false, invalidID};
        }
        float nextX = mX;
        float nextY = mY;
        float nextRowHeight = mRowHeight;
        if (nextX + rectSize.x > mSize.x) {
            nextX = 0.f;
            nextY += nextRowHeight;
            nextRowHeight = 0.f;
        }
        if (nextY + rectSize.y > mSize.y) {
            return {false, invalidID};
        }
        const size_t id = mBounds.size();
        mBounds.push_back({{nextX, nextY}, rectSize});
        mX = nextX + rectSize.x;
        mY = nextY;
        mRowHeight = std::max(nextRowHeight, rectSize.y);
        return {true, id}; 
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const { 
        return mBounds.at(rectID);
     }

private:
    sf::Vector2f mSize;
    float mX;
    float mY;
    float mRowHeight;
    std::vector<sf::FloatRect> mBounds;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, unsigned int pixels){
    const sf::Vector2u originalSize = img.getSize();
    if (originalSize.x == 0 || originalSize.y == 0) {
        throw std::invalid_argument("Cannot pad an empty image");
    }
    const sf::Vector2u paddedSize = {
        originalSize.x + 2 * pixels,
        originalSize.y + 2 * pixels
    };
    sf::Image result(paddedSize);
    for (unsigned int y = 0; y < paddedSize.y; ++y) {
        const unsigned int sourceY = y < pixels ? 0u : std::min(y - pixels, originalSize.y - 1);
        for (unsigned int x = 0; x < paddedSize.x; ++x) {
            const unsigned int sourceX = x < pixels ? 0u : std::min(x - pixels, originalSize.x - 1);
            result.setPixel({x, y}, img.getPixel({sourceX, sourceY}));
        }
    }
    return result; 
};

using TextureID = size_t;
class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) { 
        sf::Image img;
        if (!img.loadFromFile(filepath)) {
            throw std::runtime_error("Could not load image: " + filepath);
        }
        sf::Image padded = padImage(img, static_cast<unsigned int>(mPadding));
        const sf::Vector2u paddedSize = padded.getSize();
        auto [success, id] = mPacker.pack(sf::Vector2f(paddedSize));
        if (!success) {
            throw std::runtime_error("Texture atlas is full: " + filepath);
        }

        const sf::FloatRect rect = mPacker.getBounds(id);
        const sf::Vector2u position = {static_cast<unsigned int>(rect.position.x), static_cast<unsigned int>(rect.position.y)};
        mTexture.update(padded, position);
        return id;
    }

    sf::FloatRect getBoundsUV(TextureID id) const { 
        const sf::FloatRect rect = mPacker.getBounds(id);
        const sf::Vector2u atlasSize = mTexture.getSize();
        const float padding = static_cast<float>(mPadding);
        const float left = rect.position.x + padding;
        const float top = rect.position.y + padding;
        const float width = rect.size.x - 2.f * padding;
        const float height = rect.size.y - 2.f * padding;
        return { {left / static_cast<float>(atlasSize.x), 1.f - (top + height) / static_cast<float>(atlasSize.y)}, {width / static_cast<float>(atlasSize.x),height / static_cast<float>(atlasSize.y)}};
    }

    sf::Vector2u getSizePixels(TextureID id) const { 
        const sf::FloatRect rect = mPacker.getBounds(id);
        return { static_cast<unsigned int>(rect.size.x) - static_cast<unsigned int>(2 * mPadding),static_cast<unsigned int>(rect.size.y) - static_cast<unsigned int>(2 * mPadding)};
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
