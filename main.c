#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #define DEBUG

// #define FMT_NV12         // YYYYYYYYUVUV
// #define FMT_NV21         // YYYYYYYYVUVU
// #define FMT_NV16         // YYYYYYYYUVUVUVUV
// #define FMT_ARGB8888     // ARGBARGBARGBARGBARGBARGBARGBARGB

#if defined(DEBUG)
#define debug printf
#else
#define debug
#endif

#define WORD unsigned short int
#define DWORD unsigned int
#define LONG unsigned int

#define u8 unsigned char

typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;                            // 位图文件的类型，必须为BM(1-2字节)
    DWORD bfSize;                           // 位图文件的大小，以字节为单位(3-6字节，低位在前)
    WORD bfReserved1;                       // 位图文件保留字，必须为0(7-8字节)
    WORD bfReserved2;                       // 位图文件保留字，必须为0(9-10字节)
    DWORD bfOffBits;                        // 位图数据的起始位置，以相对于位图(11-14字节，低位在前)
} __attribute__((packed)) BITMAPFILEHEADER; // 文件头的偏移量表示，以字节为单位

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;         // 本结构所占用字节数(15-18字节)
    LONG biWidth;         // 位图的宽度，以像素为单位(19-22字节)
    LONG biHeight;        // 位图的高度，以像素为单位(23-26字节)
    WORD biPlanes;        // 目标设备的级别，必须为1(27-28字节)
    WORD biBitCount;      // 每个像素所需的位数，必须是1(双色)，(29-30字节) 4(16色)，8(256色)16(高彩色)或24(真彩色)之一
    DWORD biCompression;  // 位图压缩类型，必须是0(不压缩)，(31-34字节) 1(BI_RLE8压缩类型)或2(BI_RLE4压缩类型)之一
    DWORD biSizeImage;    // 位图的大小(其中包含了为了补齐行数是4的倍数而添加的空字节)，以字节为单位(35-38字节)
    LONG biXPelsPerMeter; // 位图水平分辨率，每米像素数(39-42字节)
    LONG biYPelsPerMeter; // 位图垂直分辨率，每米像素数(43-46字节)
    DWORD biClrUsed;      // 位图实际使用的颜色表中的颜色数(47-50字节)
    DWORD biClrImportant; // 位图显示过程中重要的颜色数(51-54字节)
} __attribute__((packed)) BITMAPINFOHEADER;

int parsing_main_arguments(int argc, char const *argv[]);
int parse_bmp_header(void);
int parse_bmp_raw_data(void);
int bmp_convert_to_nv12(void);
int rgb_convert_to_yuv(u8 ir, u8 ig, u8 ib, u8 *oy, u8 *ou, u8 *ov);
int generate_yuv420_image(void);

char bmp_name[16];
char out_name[16];
FILE *bmp_file = NULL;
FILE *out_file = NULL;

BITMAPFILEHEADER file_header;
BITMAPINFOHEADER info_header;

u8 *bgr_buf = NULL;
u8 *out_buf = NULL;
unsigned int bgr_buf_len = 0;
unsigned int out_buf_len = 0;

int main(int argc, char const *argv[])
{
    int ret = 0;

    ret = parsing_main_arguments(argc, argv);
    if (ret != 0)
    {
        printf("parsing_main_arguments failed\n");
        printf("\n");
        return -1;
    }

    parse_bmp_header();

    /* 申请保存原始 bitmap BGR888 格式的 buffer */
    bgr_buf_len = info_header.biSizeImage;
    bgr_buf = (u8 *)malloc(bgr_buf_len);
    memset(bgr_buf, 0, bgr_buf_len);

    /* 申请保存输出输出图片的 buffer */
#if defined(FMT_NV12)
    out_buf_len = info_header.biWidth * info_header.biHeight * 3 / 2;
#elif defined(FMT_NV21)
    out_buf_len = info_header.biWidth * info_header.biHeight * 3 / 2;
#elif defined(FMT_NV16)
    out_buf_len = info_header.biWidth * info_header.biHeight * 2;
#elif defined(FMT_ARGB8888)
    out_buf_len = info_header.biWidth * info_header.biHeight * 4;
#else
    out_buf_len = info_header.biWidth * info_header.biHeight * 3 / 2;
#endif
    out_buf = (u8 *)malloc(out_buf_len);
    memset(out_buf, 0, out_buf_len);

    parse_bmp_raw_data();
    bmp_convert_to_nv12();
    generate_yuv420_image();

    free(bgr_buf);
    free(out_buf);
    return 0;
}

/**
 * @Description 解析命令行参数到全局变量，并验证参数的有效性
 */
int parsing_main_arguments(int argc, char const *argv[])
{
    /* 打印函数使用方法 */
    if (argc != 3)
    {
        printf("usage: ./app xxx.bmp output.img\n");
        printf("eg   : ./app demo.bmp demo.nv12\n");
        printf("\n");
        return -1;
    }

    strcpy(bmp_name, argv[1]);
    strcpy(out_name, argv[2]);

    debug("bmp_name  = %s\n", bmp_name);
    debug("out_name = %s\n", out_name);

    return 0;
}

/**
 * @Description 打印 BITMAP 图像信息
 */
int parse_bmp_header(void)
{
    bmp_file = fopen(bmp_name, "r");
    if (bmp_file == NULL)
    {
        printf("err: can not open %s file\n", bmp_name);
        return -1;
    }

    debug("sizeof(WORD) = %lu\n", sizeof(WORD));
    debug("sizeof(DWORD) = %lu\n", sizeof(DWORD));
    debug("sizeof(LONG) = %lu\n", sizeof(LONG));
    debug("sizeof(BITMAPFILEHEADER) = %lu\n", sizeof(BITMAPFILEHEADER));
    debug("sizeof(BITMAPINFOHEADER) = %lu\n", sizeof(BITMAPINFOHEADER));
    debug("\n");

    fread(&file_header, 1, sizeof(BITMAPFILEHEADER), bmp_file);
    fread(&info_header, 1, sizeof(BITMAPINFOHEADER), bmp_file);
    fclose(bmp_file);

    debug("------------------------------------------------\n");
    debug(" BITMAP FILE HEADER\n");
    debug("------------------------------------------------\n");
    debug("[%lu] file_header.bfType          = 0x%04x,     %u\n", sizeof(file_header.bfType), file_header.bfType, file_header.bfType);
    debug("[%lu] file_header.bfSize          = 0x%08x, %u\n", sizeof(file_header.bfSize), file_header.bfSize, file_header.bfSize);
    debug("[%lu] file_header.bfReserved1     = 0x%04x,     %u\n", sizeof(file_header.bfReserved1), file_header.bfReserved1, file_header.bfReserved1);
    debug("[%lu] file_header.bfReserved2     = 0x%04x,     %u\n", sizeof(file_header.bfReserved2), file_header.bfReserved2, file_header.bfReserved2);
    debug("[%lu] file_header.bfOffBits       = 0x%08x, %u\n", sizeof(file_header.bfOffBits), file_header.bfOffBits, file_header.bfOffBits);
    debug("\n");

    debug("------------------------------------------------\n");
    debug(" BITMAP INFO HEADER\n");
    debug("------------------------------------------------\n");
    debug("[%lu] info_header.biSize          = 0x%08x, %u\n", sizeof(info_header.biSize), info_header.biSize, info_header.biSize);
    debug("[%lu] info_header.biWidth         = 0x%08x, %u\n", sizeof(info_header.biWidth), info_header.biWidth, info_header.biWidth);
    debug("[%lu] info_header.biHeight        = 0x%08x, %u\n", sizeof(info_header.biHeight), info_header.biHeight, info_header.biHeight);
    debug("[%lu] info_header.biPlanes        = 0x%04x,     %u\n", sizeof(info_header.biPlanes), info_header.biPlanes, info_header.biPlanes);
    debug("[%lu] info_header.biBitCount      = 0x%04x,     %u\n", sizeof(info_header.biBitCount), info_header.biBitCount, info_header.biBitCount);
    debug("[%lu] info_header.biCompression   = 0x%08x, %u\n", sizeof(info_header.biCompression), info_header.biCompression, info_header.biCompression);
    debug("[%lu] info_header.biSizeImage     = 0x%08x, %u\n", sizeof(info_header.biSizeImage), info_header.biSizeImage, info_header.biSizeImage);
    debug("[%lu] info_header.biXPelsPerMeter = 0x%08x, %u\n", sizeof(info_header.biXPelsPerMeter), info_header.biXPelsPerMeter, info_header.biXPelsPerMeter);
    debug("[%lu] info_header.biYPelsPerMeter = 0x%08x, %u\n", sizeof(info_header.biYPelsPerMeter), info_header.biYPelsPerMeter, info_header.biYPelsPerMeter);
    debug("[%lu] info_header.biClrUsed       = 0x%08x, %u\n", sizeof(info_header.biClrUsed), info_header.biClrUsed, info_header.biClrUsed);
    debug("[%lu] info_header.biClrImportant  = 0x%08x, %u\n", sizeof(info_header.biClrImportant), info_header.biClrImportant, info_header.biClrImportant);
    debug("\n");

    return 0;
}

/**
 * @Description 从 BITMAP 图像中提取出图像原始数据
 */
int parse_bmp_raw_data(void)
{
    int i;

    /* 读取 bitmap 原始图像数据 */
    bmp_file = fopen(bmp_name, "r");
    fseek(bmp_file, file_header.bfOffBits, SEEK_SET);
    fread(bgr_buf, 1, bgr_buf_len, bmp_file);

#if defined(DEBUG)
    debug("------------------------------------------------\n");
    debug(" BITMAP RAW DATA\n");
    debug("------------------------------------------------\n");
    debug(" 0x");
    for (i = 0; i < bgr_buf_len; i++)
    {
        debug("%02x", (u8)(*(bgr_buf + i)));

        if (i % 12 == 11)
            debug("\n");
        if ((i % 3 == 2) && i != bgr_buf_len - 1)
            debug(" 0x");
    }
    debug("\n");
#endif

    fclose(bmp_file);
    return 0;
}

/**
 * @Description 将 bitmap 的图像转化为 nv12 的图像
 */
int bmp_convert_to_nv12(void)
{
    u8 r, g, b;
    u8 y, u, v;
    u8 *p = NULL;
    int row;                                     // 行计数
    int col;                                     // 列计数
    int pixel_size = info_header.biBitCount / 8; // 每个像素占据的字节数

    debug("pixel_size = %d\n", pixel_size);
    debug("------------------------------------------------\n");
    debug(" RGB raw data\n");
    debug("------------------------------------------------\n");

    for (row = 0; row < info_header.biHeight; row++)
    {
        for (col = 0; col < info_header.biWidth; col++)
        {
            p = bgr_buf + (info_header.biHeight - row - 1) * info_header.biWidth * pixel_size + col * pixel_size;
            b = *p;
            g = *(p + 1);
            r = *(p + 2);
            debug(" 0x%02x%02x%02x", r, g, b);
#if defined(FMT_NV12)
            rgb_convert_to_yuv(r, g, b, &y, &u, &v);
            p = out_buf + row * info_header.biWidth + col;
            *p = y;
            if ((row % 2 == 0) && (col % 2 == 0))
            {
                p = out_buf + info_header.biWidth * info_header.biHeight + (row / 2) * info_header.biWidth + col;
                *p = u;
                *(p + 1) = v;
            }
#elif defined(FMT_NV21)
            rgb_convert_to_yuv(r, g, b, &y, &u, &v);
            p = out_buf + row * info_header.biWidth + col;
            *p = y;
            if ((row % 2 == 0) && (col % 2 == 0))
            {
                p = out_buf + info_header.biWidth * info_header.biHeight + (row / 2) * info_header.biWidth + col;
                *p = v;
                *(p + 1) = u;
            }
#elif defined(FMT_NV16)
            rgb_convert_to_yuv(r, g, b, &y, &u, &v);
            p = out_buf + row * info_header.biWidth + col;
            *p = y;
            if (col % 2 == 0)
            {
                p = out_buf + info_header.biWidth * info_header.biHeight + row * info_header.biWidth + col;
                *p = u;
                *(p + 1) = v;
            }
#elif defined(FMT_ARGB8888)
            p = out_buf + row * info_header.biWidth * 4 + col * 4;
            *p = 0xFF;
            *(p + 1) = r;
            *(p + 2) = g;
            *(p + 3) = b;
#else
            rgb_convert_to_yuv(r, g, b, &y, &u, &v);
            p = out_buf + row * info_header.biWidth + col;
            *p = y;
            if ((row % 2 == 0) && (col % 2 == 0))
            {
                p = out_buf + info_header.biWidth * info_header.biHeight + (row / 2) * info_header.biWidth + col;
                *p = u;
                *(p + 1) = v;
            }
#endif
        }
        debug("\n");
    }
    debug("\n");

#if defined(DEBUG)
    debug("------------------------------------------------\n");
    debug(" NV12 raw data\n");
    debug("------------------------------------------------\n");
    for (row = 0; row < info_header.biHeight * 3 / 2; row++)
    {
        for (col = 0; col < info_header.biWidth; col++)
        {
            p = out_buf + row * info_header.biWidth + col;
            debug(" 0x%02x", *p);
        }
        debug("\n");
    }
    debug("\n");
#endif

    return 0;
}

/**
 * @Description 将输入的 rgb 值转化为对应的 yuv 值
 */
int rgb_convert_to_yuv(u8 ir, u8 ig, u8 ib, u8 *oy, u8 *ou, u8 *ov)
{
    float r, g, b;
    float y, u, v;

    r = (float)ir;
    g = (float)ig;
    b = (float)ib;
    // debug("------------------------------------------------\n");
    // debug(" RGB convert to YUV\n");
    // debug("------------------------------------------------\n");
    // debug("r = %f\n", r);
    // debug("g = %f\n", g);
    // debug("b = %f\n", b);

#if 1
    /* CCIR 601 */
    /* RGB和YUV的范围都是[0,255] */
    y = 0.299 * r + 0.587 * g + 0.114 * b;
    u = -0.169 * r - 0.331 * g + 0.500 * b + 128;
    v = 0.500 * r - 0.419 * g - 0.081 * b + 128;
#else
    /* ISBN 1-878707-09-4 */
    /* RGB的范围是[0,255], Y的范围是[16,235], UV的范围是[16,239] */
    y = 0.257 * r + 0.504 * g + 0.098 * b + 16;
    u = -0.148 * r - 0.291 * g + 0.439 * b + 128;
    v = 0.439 * r - 0.368 * g - 0.071 * b + 128;
#endif

    /* float 类型强转为整形，做四舍五入转化的话，原本是要加 0.5 的 */
    /* 但是由于 FF 的颜色值，计算到的 YUV 分量的值为 255.5, 再加 0.5 后数据会溢出 */
    /* 因此这里迫不得已加 0.4999 将就一下 */
    *oy = (u8)(y + 0.4999);
    *ou = (u8)(u + 0.4999);
    *ov = (u8)(v + 0.4999);
    // debug("y = 0x%x(h), %d(d), %f(f)\n", *oy, *oy, y);
    // debug("u = 0x%x(h), %d(d), %f(f)\n", *ou, *ou, u);
    // debug("v = 0x%x(h), %d(d), %f(f)\n", *ov, *ov, v);

    return 0;
}

/**
 * @Description 生成一个 nv12 图片
 */
int generate_yuv420_image(void)
{
    out_file = fopen(out_name, "wb");
    if (out_file == NULL)
    {
        printf("err: can not open %s file\n", out_name);
        return -1;
    }

    fwrite(out_buf, 1, out_buf_len, out_file);
    fclose(out_file);

    return 0;
}
