/**
 * \file
 * \brief Source file for doing alpha blending
*/

#include <SFML/Graphics.hpp>
#include <assert.h>
#include <memory.h>
#include "configs.hpp"
#include "draw.hpp"


typedef struct {
    uint32_t r = 0;
    uint32_t g = 0;
    uint32_t b = 0;
    uint32_t a = 0;
} PixelColor;


const uint8_t ZERO = 255u;      /// Zero value in shuffle function
const size_t BUF_ALIGN = 32;    /// Pixel array required alignment


/**
 * \brief Alpha blends foreground and background pixels and store result in buffer
 * \param [out] buffer  Buffer to store pixels colors
 * \param [in]  front   Foreground image
 * \param [in]  back    Background image
*/
void blend_pixels(uint8_t *buffer, const uint8_t *front, const uint8_t *back);


/**
 * \brief Copies previous image pixels to new image with specified size with the given offset
 * \param [in]  prev_image  Previous image pixels
 * \param [in]  prev_width  Previous image width
 * \param [in]  prev_height Previous image height
 * \param [in]  offset      Previous image pixels offset in new image
 * \param [in]  new_width   New image width
 * \param [in]  new_height  New image height
 * \return Pointer to new image pixels buffer or nullptr if creation failed
*/
uint8_t *resize_image(const uint8_t *prev_image, size_t prev_width, size_t prev_height, size_t offset, size_t new_width, size_t new_height);


/**
 * \brief Loads foreground and background images to memory and resizes front image to SCREEN_W x SCREEN_H size
 * \param [in]  front_filename  Path to foreground image
 * \param [in]  back_filename   Path to background image
 * \param [out] front_buffer    Buffer to store resized front image pixels
 * \param [out] back_buffer     Buffer to store back image pixels
 * \note In case of error function free all already allocated arrays
 * \return Non zero value means error
*/
int load_images(const char *front_filename, const char *back_filename, uint8_t **front_buffer, uint8_t **back_buffer);




int blend_images(const char *front, const char *back) {
    assert(front && "Path to foreground image is null ptr!\n");
    assert(back && "Path to background image is null ptr!\n");

    sf::RenderWindow window(sf::VideoMode(SCREEN_W, SCREEN_H), "AlphaBlending3000");


    sf::Font font;
    if (!font.loadFromFile(FONT_FILE)) {
        printf("Can't open %s!\n", FONT_FILE);
        return 1;
    }

    sf::Text status(sf::String("FPS: 0"), font, FONT_SIZE);
    status.setFillColor(sf::Color().Black);


    sf::Clock clock;
    sf::Time prev_time = clock.getElapsedTime();
    sf::Time curr_time = sf::seconds(0);
    char fps_text[30] = "";


    uint8_t *pixels = (uint8_t *) aligned_alloc(BUF_ALIGN, SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t));
    if (!pixels) {
        printf("Can't allocate buffer for pixels colors!\n");
        return 1;
    }


    uint8_t *front_pixels = nullptr, *back_pixels = nullptr;
    if (load_images(front, back, &front_pixels, &back_pixels)) {
        free(pixels);
        return 1;
    }


    sf::Image tool_image;
    sf::Texture tool_texture;


    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }


        for (size_t i = 0; i < 10000; i++) blend_pixels(pixels, front_pixels, back_pixels);
        tool_image.create(SCREEN_W, SCREEN_H, pixels);
        tool_texture.loadFromImage(tool_image);
        sf::Sprite tool_sprite(tool_texture);


        window.clear();
        window.draw(tool_sprite);
        window.draw(status);
        window.display();


        curr_time = clock.getElapsedTime();
        int fps = (int)(10000.0f / (curr_time.asSeconds() - prev_time.asSeconds()));
        sprintf(fps_text, "FPS: %i", fps);
        status.setString(fps_text);
        prev_time = curr_time;
    }

    free(pixels);
    free(front_pixels);
    free(back_pixels);

    return 0;
}


void blend_pixels(uint8_t *buffer, const uint8_t *front, const uint8_t *back) {
    assert(buffer && "Can't set pixels with null buffer!\n");
    assert(front && "Can't work with null front buffer!\n");
    assert(back && "Can't work with null back buffer!\n");


    assert(!((unsigned long long) buffer & 0x1F) && "Buffer has invalid alignment!\n");
    assert(!((unsigned long long) front & 0x1F) && "Front buffer has invalid alignment!\n");
    assert(!((unsigned long long) back & 0x1F) && "Back buffer has invalid alignment!\n");


    for (uint32_t y = 0; y < SCREEN_H; y++) {
        for (uint32_t x = 0; x < SCREEN_W; x ++) {
            PixelColor fr = {front[0], front[1], front[2], front[3]};
            PixelColor bg = {back[0], back[1], back[2], back[3]};

            PixelColor buf = {
                (fr.r * fr.a + bg.r * (255u - fr.a)) >> 8u,
                (fr.g * fr.a + bg.g * (255u - fr.a)) >> 8u,
                (fr.b * fr.a + bg.b * (255u - fr.a)) >> 8u,
                255u
            };

            buffer[0] = (uint8_t) buf.r, buffer[1] = (uint8_t) buf.g, buffer[2] = (uint8_t) buf.b, buffer[3] = (uint8_t) buf.a;

            front += sizeof(int);
            back += sizeof(int);
            buffer += sizeof(int);
        }
    }
}


uint8_t *resize_image(const uint8_t *prev_image, size_t prev_width, size_t prev_height, size_t offset, size_t new_width, size_t new_height) {
    uint8_t *new_image = (uint8_t *) aligned_alloc(BUF_ALIGN, new_width * new_height * 4 * sizeof(uint8_t));
    if (!new_image) return nullptr;

    for (size_t y = 0; y < prev_height; y++)
        memcpy(new_image + (offset + y * new_width) * 4, prev_image + (y * prev_width) * 4, 4 * prev_width);

    return new_image;
}


int load_images(const char *front_filename, const char *back_filename, uint8_t **front_buffer, uint8_t **back_buffer) {
    sf::Image tool_image;

    tool_image.loadFromFile(front_filename);

    if (!tool_image.getPixelsPtr()){
        printf("%s not found!\n", front_filename);
        return 1;
    }

    *front_buffer = resize_image(tool_image.getPixelsPtr(), tool_image.getSize().x, tool_image.getSize().y, 175400, SCREEN_W, SCREEN_H);

    if (!(*front_buffer)) {
        printf("Can't allocate buffer for foreground image pixels!\n");
        return 1;
    }

    tool_image.loadFromFile(back_filename);

    if (!tool_image.getPixelsPtr()){
        free(*front_buffer);
        *front_buffer = nullptr;

        printf("%s not found!\n", back_filename);
        return 1;
    }

    *back_buffer = (uint8_t *) aligned_alloc(BUF_ALIGN, SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t));

    if (!(*back_buffer)) {
        free(*front_buffer);
        *front_buffer = nullptr;

        printf("Can't allocate buffer for background image pixels!\n");
        return 1;
    }

    memcpy(*back_buffer, tool_image.getPixelsPtr(), SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t));

    return 0;
}
