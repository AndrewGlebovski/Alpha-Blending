#include <stdio.h>
#include "configs.hpp"
#include "draw.hpp"


int main() {
    int result = blend_images(FRONT_IMAGE, BACK_IMAGE);

    printf("Alpha Blending!\n");

    return result;
}
