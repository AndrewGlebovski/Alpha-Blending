/**
 * \file
 * \brief Source file for doing alpha blending
*/

#include <SFML/Graphics.hpp>
#include <assert.h>
#include <immintrin.h>
#include <memory.h>
#include "configs.hpp"
#include "draw.hpp"


const uint8_t ZERO = 255u;  /// Zero value in shuffle function


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


    uint8_t *pixels = (uint8_t *) calloc(SCREEN_W * SCREEN_H * 4, sizeof(uint8_t));

    if (!pixels) {
        printf("Can't allocate buffer for pixels colors!\n");
        return 1;
    }


    sf::Image img1, img2;
    img1.loadFromFile(front);
    img2.loadFromFile(back);

    if (!(img1.getPixelsPtr() && img2.getPixelsPtr())){
        printf("%s not found!\n", (!img1.getPixelsPtr()) ? front : back);
        return 1;
    }

    img1.create(SCREEN_W, SCREEN_H, resize_image(img1.getPixelsPtr(), img1.getSize().x, img1.getSize().y, 175400, SCREEN_W, SCREEN_H));

    sf::Image image;
    sf::Texture texture;


    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }


        blend_pixels(pixels, img1.getPixelsPtr(), img2.getPixelsPtr());
        image.create(SCREEN_W, SCREEN_H, pixels);
        texture.loadFromImage(image);
        sf::Sprite sprite(texture);


        curr_time = clock.getElapsedTime();
        int fps = (int)(1.0f / (curr_time.asSeconds() - prev_time.asSeconds()));
        sprintf(fps_text, "FPS: %i", fps);
        status.setString(fps_text);
        prev_time = curr_time;


        window.clear();
        window.draw(sprite);
        window.draw(status);
        window.display();
    }

    free(pixels);

    return 0;
}


void blend_pixels(uint8_t *buffer, const uint8_t *front, const uint8_t *back) {
    assert(buffer && "Can't set pixels with null buffer!\n");
    assert(front && "Can't work with null foreground buffer!\n");
    assert(back && "Can't work with null background buffer!\n");


    for (uint32_t y = 0; y < SCREEN_H; y++) {
        for (uint32_t x = 0; x < SCREEN_W; x += 8) {
            __m256i front_org = _mm256_loadu_si256((const __m256i *) front);
            __m256i back_org = _mm256_loadu_si256((const __m256i *) back);


            __m256i front_l = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(front_org, 1));
            __m256i front_h = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(front_org, 0));

            __m256i back_l = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(back_org, 1));
            __m256i back_h = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(back_org, 0));


            __m256i shuffle_mask = _mm256_set_epi8(
                ZERO, 14, ZERO, 14, ZERO,  14, ZERO,  14,  ZERO,  6,  ZERO,  6,  ZERO,  6,  ZERO,  6,
                ZERO, 14, ZERO, 14, ZERO,  14, ZERO,  14,  ZERO,  6,  ZERO,  6,  ZERO,  6,  ZERO,  6
            );


            __m256i alpha_l = _mm256_shuffle_epi8(front_l, shuffle_mask);
            __m256i alpha_h = _mm256_shuffle_epi8(front_h, shuffle_mask);;


            front_l = _mm256_mullo_epi16(front_l, alpha_l);
            front_h = _mm256_mullo_epi16(front_h, alpha_h);


            alpha_l = _mm256_subs_epu16(_mm256_set1_epi16(255), alpha_l);
            alpha_h = _mm256_subs_epu16(_mm256_set1_epi16(255), alpha_h);


            back_l = _mm256_mullo_epi16(back_l, alpha_l);
            back_h = _mm256_mullo_epi16(back_h, alpha_h);


            __m256i sum_l = _mm256_add_epi16(front_l, back_l);
            __m256i sum_h = _mm256_add_epi16(front_h, back_h);


            shuffle_mask = _mm256_set_epi8(
                  15,   13,   11,    9,    7,    5,    3,    1, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO,
                ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO,   15,   13,   11,    9,    7,    5,    3,    1
            );


            sum_l = _mm256_shuffle_epi8(sum_l, shuffle_mask);
            sum_h = _mm256_shuffle_epi8(sum_h, shuffle_mask);


            __m256i colors = _mm256_set_m128i(  /// IT'S REVERSED CAUSE VECTOR REVERSE ARRAY VALUES
                _mm_add_epi8(_mm256_extracti128_si256(sum_l, 0), _mm256_extracti128_si256(sum_l, 1)),
                _mm_add_epi8(_mm256_extracti128_si256(sum_h, 0), _mm256_extracti128_si256(sum_h, 1))
            );
            
 
            _mm256_storeu_si256((__m256i *) buffer, colors);


            front += 8 * sizeof(int);
            back += 8 * sizeof(int);
            buffer += 8 * sizeof(int);
        }
    }
}


uint8_t *resize_image(const uint8_t *prev_image, size_t prev_width, size_t prev_height, size_t offset, size_t new_width, size_t new_height) {
    uint8_t *new_image = (uint8_t *) calloc(new_width * new_height * 4, sizeof(uint8_t));
    assert(new_image && "Couldn't allocate buffer for new image!\n");

    for (size_t y = 0; y < prev_height; y++)
        memcpy(new_image + (offset + y * new_width) * 4, prev_image + (y * prev_width) * 4, 4 * prev_width);

    return new_image;
}
