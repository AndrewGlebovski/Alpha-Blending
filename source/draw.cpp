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


const uint8_t ZERO = 255u;      /// Zero value in shuffle function
const size_t BUF_ALIGN = 32;    /// Pixel array required alignment


typedef enum {
    OK,                         ///< OK
    FILE_NOT_FOUND,             ///< File does not exist
    ALLOC_FAIL,                 ///< Failed to allocate memory
    ALIGN_FAIL                  ///< Memory is not aligned
} EXIT_CODES;


#define ASSERT(condition, exitcode, ...)                \
do {                                                    \
    if (!(condition)) {                                 \
        printf(__VA_ARGS__);                            \
        return exitcode;                                \
    }                                                   \
} while(0)


#define CHECK_ALIGN(ptr, ...)                                   \
do {                                                            \
    if (ptr && (unsigned long long) ptr & (BUF_ALIGN - 1)) {    \
        __VA_ARGS__                                             \
    }                                                           \
} while(0)


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
    ASSERT(font.loadFromFile(FONT_FILE), FILE_NOT_FOUND, "Can't open %s!\n", FONT_FILE);

    sf::Text status(sf::String("FPS: 0"), font, FONT_SIZE);
    status.setFillColor(sf::Color().Black);


    sf::Clock clock;
    sf::Time prev_time = clock.getElapsedTime();
    sf::Time curr_time = sf::seconds(0);
    char fps_text[30] = "";


    uint8_t *pixels = (uint8_t *) aligned_alloc(BUF_ALIGN, SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t));
    ASSERT(pixels, FILE_NOT_FOUND, "Can't allocate buffer for pixels colors!\n");
    CHECK_ALIGN(pixels,
        free(pixels);

        printf("Buffer for pixels is not aligned!\n");
        return ALIGN_FAIL;
    );


    uint8_t *front_pixels = nullptr, *back_pixels = nullptr;
    int error = load_images(front, back, &front_pixels, &back_pixels);
    if (error) {
        free(pixels);
        return error;
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


    assert(!((unsigned long long) buffer & (BUF_ALIGN - 1)) && "Buffer has invalid alignment!\n");
    assert(!((unsigned long long) front & (BUF_ALIGN - 1)) && "Front buffer has invalid alignment!\n");
    assert(!((unsigned long long) back & (BUF_ALIGN - 1)) && "Back buffer has invalid alignment!\n");


    for (uint32_t y = 0; y < SCREEN_H; y++) {
        for (uint32_t x = 0; x < SCREEN_W; x += 8) {
            __m256i front_org = _mm256_load_si256((const __m256i *) front);
            __m256i back_org = _mm256_load_si256((const __m256i *) back);


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


            __m256i colors = _mm256_set_m128i(
                _mm_add_epi8(_mm256_extracti128_si256(sum_l, 0), _mm256_extracti128_si256(sum_l, 1)),
                _mm_add_epi8(_mm256_extracti128_si256(sum_h, 0), _mm256_extracti128_si256(sum_h, 1))
            );
            
 
            _mm256_store_si256((__m256i *) buffer, colors);


            front += 8 * sizeof(int);
            back += 8 * sizeof(int);
            buffer += 8 * sizeof(int);
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

    ASSERT(tool_image.getPixelsPtr(), FILE_NOT_FOUND, "%s not found!\n", front_filename);

    *front_buffer = resize_image(tool_image.getPixelsPtr(), tool_image.getSize().x, tool_image.getSize().y, 175400, SCREEN_W, SCREEN_H);

    ASSERT(*front_buffer, ALLOC_FAIL, "Can't allocate buffer for foreground image pixels!\n");
    CHECK_ALIGN(*front_buffer,
        free(front_buffer);
        *front_buffer = nullptr;

        printf("Buffer for front pixels is not aligned!\n");
        return ALIGN_FAIL;
    );

    tool_image.loadFromFile(back_filename);

    if (!tool_image.getPixelsPtr()){
        free(*front_buffer);
        *front_buffer = nullptr;

        printf("%s not found!\n", back_filename);
        return FILE_NOT_FOUND;
    }

    *back_buffer = (uint8_t *) aligned_alloc(BUF_ALIGN, SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t));

    if (!(*back_buffer)) {
        free(*front_buffer);
        *front_buffer = nullptr;

        printf("Can't allocate buffer for background image pixels!\n");
        return ALLOC_FAIL;
    }
    CHECK_ALIGN(*back_buffer,
        free(*front_buffer);
        *front_buffer = nullptr;

        free(*back_buffer);
        *back_buffer = nullptr;

        printf("Buffer for background pixels is not aligned!\n");
        return ALIGN_FAIL;
    );

    memcpy(*back_buffer, tool_image.getPixelsPtr(), SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t));

    return 0;
}
