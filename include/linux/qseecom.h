/*
 * MAX_APP_NAME_SIZE / QSEECOM_HASH_SIZE 等宏在 4.19 里定义于
 * include/uapi/linux/qseecom.h。qseecomi.h 只 include 了本文件，
 * 所以这里把 uapi 版本带进来（不会重复定义，uapi 有 header guard）。
 */
#include <uapi/linux/qseecom.h>
