/**
 * \file
 * \brief Header file for doing alpha blending
*/


/**
 * \brief Constantly blends using alpha channel
 * \param [in] front    Foreground image
 * \param [in] back     Background image
 * \return Non zero value means error
*/
int blend_images(const char *front, const char *back);
