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


#ifndef OPTI

typedef struct {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
} PixelColor;

#endif


const uint8_t ZERO = 255u;          ///< Zero value in shuffle function
const size_t BUF_ALIGN = 32;        ///< Pixel array required alignment
const size_t TEST_NUMBER = 8;

const size_t IMG_BUFFER_SIZE = SCREEN_W * SCREEN_H * 4 * sizeof(uint8_t);   ///< Standart buffer size for all images


typedef enum {
    OK,                         ///< OK
    FILE_NOT_FOUND,             ///< File does not exist
    ALLOC_FAIL,                 ///< Failed to allocate memory or allocated memory was not aligned properly
    MEM_FAIL                    ///< Operation with memory failed
} EXIT_CODES;


typedef struct {
    sf::Window  *window = nullptr;      ///< Application window
} EventArgs;


#define ASSERT(condition, exitcode, ...)                \
do {                                                    \
    if (!(condition)) {                                 \
        printf(__VA_ARGS__);                            \
        return exitcode;                                \
    }                                                   \
} while(0)


#define REPEAT(condition, ...)                          \
do {                                                    \
    int exitcode = (condition);                         \
    if (exitcode) {                                     \
        __VA_ARGS__                                     \
        return exitcode;                                \
    }                                                   \
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


/**
 * \brief Handles all types of events
 * \param [in,out] args Contains all necessary arguments
 * \return Non zero value means error 
*/
int event_parser(EventArgs *args);


/**
 * \brief Allocates alligned memory
 * \param [out] buffer  Save allocated memory address
 * \param [in]  size    Required memory size
 * \return Non zero value means error
*/
int safe_aligned_alloc(uint8_t **buffer, size_t size);




int blend_images(const char *front, const char *back) {
    assert(front && "Path to foreground image is null ptr!\n");
    assert(back && "Path to background image is null ptr!\n");

    sf::RenderWindow window(sf::VideoMode(SCREEN_W, SCREEN_H), "AlphaBlending3000");

    sf::Font font;
    ASSERT(font.loadFromFile(FONT_FILE), FILE_NOT_FOUND, "Can't open %s!\n", FONT_FILE);

    uint8_t *pixels = nullptr;
    REPEAT(safe_aligned_alloc(&pixels, IMG_BUFFER_SIZE));

    uint8_t *front_pixels = nullptr, *back_pixels = nullptr;
    REPEAT(load_images(front, back, &front_pixels, &back_pixels), free(pixels););

    sf::Image tool_image;
    sf::Texture tool_texture;

    blend_pixels(pixels, front_pixels, back_pixels);

    tool_image.create(SCREEN_W, SCREEN_H, pixels);
    tool_texture.loadFromImage(tool_image);
    sf::Sprite tool_sprite(tool_texture);
    
    EventArgs event_args = {&window};

    while (window.isOpen()) {
        if (event_parser(&event_args)) break;

        window.clear();
        window.draw(tool_sprite);
        window.display();
    }
    
    free(pixels);
    free(front_pixels);
    free(back_pixels);

    return OK;
}


int safe_aligned_alloc(uint8_t **buffer, size_t size) {
    if (!buffer) return ALLOC_FAIL;

    *buffer = (uint8_t *) aligned_alloc(BUF_ALIGN, size);

    if (!*buffer) {
        return ALLOC_FAIL;
    }

    if ((unsigned long long) *buffer & (BUF_ALIGN - 1)) {
        free(*buffer), *buffer = nullptr;
        return ALLOC_FAIL;
    }

    return OK;
}


int event_parser(EventArgs *args) {
    assert(args && "Can't parse events without args!\n");

    sf::Event event;
    while (args -> window -> pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            args -> window -> close();
            return 1;
        }
    }

    return 0;
}


void blend_pixels(uint8_t *buffer, const uint8_t *front, const uint8_t *back) {
    assert(buffer && "Can't set pixels with null buffer!\n");
    assert(front && "Can't work with null front buffer!\n");
    assert(back && "Can't work with null back buffer!\n");

    assert(!((unsigned long long) buffer & (BUF_ALIGN - 1)) && "Buffer has invalid alignment!\n");
    assert(!((unsigned long long) front & (BUF_ALIGN - 1)) && "Front buffer has invalid alignment!\n");
    assert(!((unsigned long long) back & (BUF_ALIGN - 1)) && "Back buffer has invalid alignment!\n");

    sf::Clock clock;

    for (uint32_t y = 0; y < SCREEN_H; y++) {
        #ifdef OPTI

            for (uint32_t x = 0; x < SCREEN_W; x += 8) {
                __m256i front_org = _mm256_load_si256((const __m256i *) front);
                __m256i back_org = _mm256_load_si256((const __m256i *) back);
                __m256i colors = _mm256_set1_epi32(0);

                for (size_t i = 0; i < TEST_NUMBER * 8; i++) {
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


                    colors = _mm256_set_m128i(
                        _mm_add_epi8(_mm256_extracti128_si256(sum_l, 0), _mm256_extracti128_si256(sum_l, 1)),
                        _mm_add_epi8(_mm256_extracti128_si256(sum_h, 0), _mm256_extracti128_si256(sum_h, 1))
                    );
                }
    
                _mm256_store_si256((__m256i *) buffer, colors);

                front   += 8 * sizeof(int);
                back    += 8 * sizeof(int);
                buffer  += 8 * sizeof(int);
            }

        #else 

            for (uint32_t x = 0; x < SCREEN_W; x++) {
                PixelColor fr = {}; memcpy(&fr, front, 4);
                PixelColor bg = {}; memcpy(&bg, back, 4);

                unsigned int alpha = fr.a;

                for (size_t i = 0; i < TEST_NUMBER; i++) {
                    fr = {
                        (uint8_t) ((fr.r * alpha + bg.r * (255u - alpha)) >> 8),
                        (uint8_t) ((fr.g * alpha + bg.g * (255u - alpha)) >> 8),
                        (uint8_t) ((fr.b * alpha + bg.b * (255u - alpha)) >> 8),
                        255u
                    };
                }

                memcpy(buffer, &fr, 4);

                front += sizeof(int);
                back += sizeof(int);
                buffer += sizeof(int);
            }

        #endif
    }

    printf("%f\n", clock.getElapsedTime().asSeconds());
}


uint8_t *resize_image(const uint8_t *prev_image, size_t prev_width, size_t prev_height, size_t offset, size_t new_width, size_t new_height) {
    uint8_t *new_image = nullptr;
    if (safe_aligned_alloc(&new_image, new_width * new_height * 4 * sizeof(uint8_t))) return nullptr;

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

    tool_image.loadFromFile(back_filename);

    if (!tool_image.getPixelsPtr()){
        free(*front_buffer), *front_buffer = nullptr;

        printf("%s not found!\n", back_filename);
        return FILE_NOT_FOUND;
    }

    REPEAT(safe_aligned_alloc(back_buffer, IMG_BUFFER_SIZE), free(*front_buffer); *front_buffer = nullptr;);

    ASSERT(memcpy(*back_buffer, tool_image.getPixelsPtr(), IMG_BUFFER_SIZE), MEM_FAIL, "Memcopy failed!\n");

    return OK;
}
