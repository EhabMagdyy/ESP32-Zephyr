#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>

#define TEST_FILE   "/lfs/stm32.bin"

// Small fake "firmware" payload for testing
static const uint8_t fake_firmware[] = {
    0xDE, 0xAD, 0xBE, 0xEF,
    0x01, 0x02, 0x03, 0x04,
    0xAA, 0xBB, 0xCC, 0xDD,
};

static int test_write(void){
    struct fs_file_t file;
    fs_file_t_init(&file);

    int ret = fs_open(&file, TEST_FILE, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if(ret < 0){
        printk("ERROR: fs_open(write) failed: %d\n", ret);
        return ret;
    }

    ssize_t written = fs_write(&file, fake_firmware, sizeof(fake_firmware));
    if(written < 0){
        printk("ERROR: fs_write failed: %d\n",(int)written);
        fs_close(&file);
        return(int)written;
    }

    printk("Wrote %d bytes to %s\n",(int)written, TEST_FILE);
    fs_close(&file);
    return 0;
}

static int test_read_back(void){
    struct fs_file_t file;
    fs_file_t_init(&file);

    int ret = fs_open(&file, TEST_FILE, FS_O_READ);
    if(ret < 0){
        printk("ERROR: fs_open(read) failed: %d\n", ret);
        return ret;
    }

    uint8_t buf[sizeof(fake_firmware)];
    ssize_t nread = fs_read(&file, buf, sizeof(buf));
    if(nread < 0){
        printk("ERROR: fs_read failed: %d\n",(int)nread);
        fs_close(&file);
        return(int)nread;
    }

    printk("Read back %d bytes:\n",(int)nread);
    for(int i = 0; i < nread; i++){
        printk("  [%02d] wrote=0x%02X  read=0x%02X  %s\n", i, fake_firmware[i], buf[i], (buf[i] == fake_firmware[i]) ? "OK" : "MISMATCH");
    }

    fs_close(&file);

    /* Verify */
    if(nread != sizeof(fake_firmware) || memcmp(buf, fake_firmware, sizeof(fake_firmware)) != 0){
        printk("RESULT: FAIL - data mismatch\n");
        return -EIO;
    }

    printk("RESULT: PASS - data verified\n");
    return 0;
}

static int test_stat(void){
    struct fs_dirent dirent;

    int ret = fs_stat(TEST_FILE, &dirent);
    if(ret < 0){
        printk("ERROR: fs_stat failed: %d\n", ret);
        return ret;
    }

    printk("stat: name=%s  size=%zu  type=%s\n", dirent.name, dirent.size, dirent.type == FS_DIR_ENTRY_FILE ? "FILE" : "DIR");
    return 0;
}

static int test_delete(void){
    int ret = fs_unlink(TEST_FILE);
    if(ret < 0){
        printk("ERROR: fs_unlink failed: %d\n", ret);
        return ret;
    }
    printk("Deleted %s\n", TEST_FILE);
    return 0;
}

int main(void){
    printk("=== LittleFS test start ===\n");

    /* If automount is set in DTS the FS is already mounted.
       Manual mount only needed if you didn't use fstab/automount. */

    int ret;

    ret = test_write();
    if(ret){
		printk("=== LittleFS test FAILED(ret=%d) ===\n", ret);
		goto done;
	}
    ret = test_stat();
    if(ret){
		printk("=== LittleFS test FAILED(ret=%d) ===\n", ret);
		goto done;
	}

    ret = test_read_back();
    if(ret){
		printk("=== LittleFS test FAILED(ret=%d) ===\n", ret);
		goto done;
	}

    ret = test_delete();
    if(ret){
		printk("=== LittleFS test FAILED(ret=%d) ===\n", ret);
		goto done;
	}

	done: 
    printk("=== LittleFS test PASSED ===\n");

	while(1){
		// make this thread sleeps forever
		k_sleep(K_FOREVER);
	}
	return 0;
}
