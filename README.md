### 程序说明

将 bitmap 格式的图片转化为 NV12 格式。

命令格式: `./app <input bitmap file name> <ouput nv12 file name>`

### 编译执行

    user@ubuntu:~/work/c-study/Extract-the-original-image-data-in-bitmap$ make
    gcc -o app main.c
    user@ubuntu:~/work/c-study/Extract-the-original-image-data-in-bitmap$ ./app demo.bmp out.nv12


