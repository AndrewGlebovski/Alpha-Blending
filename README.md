# Alpha Blending


![result](assets/result.png "Результат блендинга")


## Использование


Скомпилировать проект можно с помощью команды (**вам потребуется библиотека sfml**)
```
make
```

При запуске программа наложит изображение *assets/AskhatCat.bmp* на *assets/Table.bmp* и выведет результат в окно размером *800x600*. *Размер изображений фиксирован.*


## Цель


Закрепить умение оптимизировать однотипные вычисления на CPU с помощью SIMD (см. также [Mandelbrot-Set](https://github.com/AndrewGlebovski/Mandelbrot-Set)).


## Введение


Alpha Blending - процесс наложения изображения на фоновое изображение с учетом прозрачности. Цвета каждого пикселя вычисляется по следующей формуле

```
pixel.red   = (foreground_pixel.red   * foreground_pixel.alpha + background_pixel.red   * (255 - foreground_pixel.alpha)) << 8

pixel.green = (foreground_pixel.green * foreground_pixel.alpha + background_pixel.green * (255 - foreground_pixel.alpha)) << 8

pixel.blue  = (foreground_pixel.blue  * foreground_pixel.alpha + background_pixel.blue  * (255 - foreground_pixel.alpha)) << 8

pixel.alpha = 255
```

Т.к. пиксели вычисляются независимо друг от друга, то можно подсчитать сразу несколько с помощью SIMD.


## Ход работы


- Будем вычислять цвет одного пикселя за одну итерацию внутреннего цикла.
- Воспользуемся SIMD и будем считать по 8 пикселей за итерацию (команды AVX2 используют регистры на 256 бит, при этом каждый пиксель занимает 4 байта, значит в один регистр помещается 8 int чисел, т.е. 8 пикселей)
- Замерим и сравним время в обоих случаях.

Стоит отметить одну особенность в исполнении. Т.к. при умножении целых чисел может возникнуть переполнение, то нужно разбить наш 256-битный регистр пополам на два 128-битных, расширить размер каждого операнда с одного байта до двух (*команда _mm256_cvtepu8_epi16*) и производить арифметические операции с ними. После уменьшить каждый операнд с двух байт до одного, взяв старший байт, и "склеить" получившиеся 128-битные регистры.


## Анализ работы компилятора


С помощью сайта godbolt.org проанализируем существенные различия при компиляции "наивной" и SIMD реализаций.

[Результат для наивной реализации](https://godbolt.org/z/faMKhW3T6)

[Результат для SIMD реализации](https://godbolt.org/z/nchY8YYrT)

Наивный алгоритм производит 3 сложения и 6 умножений над каждым пикселем. SIMD производит все арифметический команды над 8 пикселями сразу.

| ![naive-cmp](assets/simd_asm.png)  | ![naive-cmp](assets/naive_asm.png) |
| ---------------------------------- | ---------------------------------- |
| SIMD реализация                    | Наивная реализация                 |


## Результат


**Условия**
- Все измерения производились с опцией -O2 компилятора gcc, а также набором инструкций AVX2.
- Для того, чтобы увеличить влияние арифмитических вычислений, все вычисления умножались на 8.
- Компьютер был подключен к сети, чтобы система не снимала нагрузку с CPU для экономии заряда.
- Замеры проводились при одной температуре CPU (44.2), чтобы избежать искажения показаний из-за троттлинга CPU.

| SIMD | Время (секунды)      | Среднеквадратичное отклонение |
| ---- | -------------------- | ----------------------------- |
| Off  | 0.013                | 0.002                         |
| On   | 0.0018               | 0.0004                        |

Коэффициент ускорения рассчитывается по формуле $\frac{FPSWithSIMD}{FPSWithoutSIMD}$.

|                | Коэффициент ускорения |
| -------------- | --------------------- |
| Теоретический  | 8                     |
| Практический   | 7.22                  |


## Вывод


Использование SIMD позволяет свернуть несколько итераций цикла в одну, если каждая итерация выполняется независимо от других, что позволяет существенно ускорить выполнение цикла. Для этого используются специальные ассемблерные вставки, поддерживаемые компилятором (интринсики).
