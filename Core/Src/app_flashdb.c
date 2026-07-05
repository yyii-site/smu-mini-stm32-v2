// home/yi/Code/stm32/stm32Proj/Middlewares/Third_Party/FlashDB/demos/stm32f405rg/applications/main.c
#include "app_flashdb.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define FDB_LOG_TAG "[main]"

// 互斥锁控制
static SemaphoreHandle_t fdb_mutex = NULL;

// 数据库内部变量与默认表
static uint32_t boot_count = 0;
static time_t boot_time[10] = {0, 1, 2, 3};

static struct fdb_default_kv_node default_kv_table[] = {
    {"username", "armink", 0},
    {"password", "123456", 0},
    {"boot_count", &boot_count, sizeof(boot_count)},
    {"boot_time", &boot_time, sizeof(boot_time)},
};

/* KVDB object */
struct fdb_kvdb kvdb = {0};
/* TSDB object */
struct fdb_tsdb tsdb = {0};

// 定义外部示例函数（仅在此文件内部使用）
// extern void kvdb_basic_sample(fdb_kvdb_t kvdb);
// extern void kvdb_type_string_sample(fdb_kvdb_t kvdb);
// extern void kvdb_type_blob_sample(fdb_kvdb_t kvdb);
// extern void tsdb_sample(fdb_tsdb_t tsdb);

// 锁与时间回调实现
static void lock(fdb_db_t db)
{
    if (fdb_mutex == NULL)
    {
        fdb_mutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(fdb_mutex, portMAX_DELAY);
}

static void unlock(fdb_db_t db)
{
    if (fdb_mutex != NULL)
    {
        xSemaphoreGive(fdb_mutex);
    }
}

// 统一初始化接口
fdb_err_t app_flashdb_init(void)
{
    // 首先初始化绑定的 FAL 抽象层
    fal_init();

    // 设置锁和时间回调
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, (void *)lock);
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, (void *)unlock);

    /* KVDB Sample */
    struct fdb_default_kv default_kv;

    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    /* Key-Value database initialization
     *
     *       &kvdb: database object
     *       "env": database name
     * "fdb_kvdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
     *              Please change to YOUR partition name.
     * &default_kv: The default KV nodes. It will auto add to KVDB when first initialize successfully.
     *        NULL: The user data if you need, now is empty.
     */

    // 初始化 KV 数据库，传入默认表
    return fdb_kvdb_init(&kvdb, "env", "fdb_kvdb1", &default_kv, NULL);
}

// 7. 运行示例接口
void app_flashdb_run_samples(void)
{
    struct fdb_blob blob;
    uint32_t count = 0;

    // 【测试 1】：尝试从数据库读取 "boot_count" 的值
    fdb_kv_get_blob(&kvdb, "boot_count", fdb_blob_make(&blob, &count, sizeof(count)));
    
    // 如果是第一次开机，读取出来的 count 会是 0（因为默认表里写的是 0）
    // 每次复位，我们让它自增 1
    count++;

    // 【测试 2】：尝试将自增后的值写回 Flash
    fdb_kv_set_blob(&kvdb, "boot_count", fdb_blob_make(&blob, &count, sizeof(count)));

    // 【测试 3】：打印出来看看成功没有
    FDB_PRINT("========================================\n");
    FDB_PRINT("FlashDB 自检测试: 当前开机计数 = %d\n", (int)count);
    FDB_PRINT("========================================\n");
}