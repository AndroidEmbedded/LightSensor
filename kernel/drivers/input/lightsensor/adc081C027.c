/*
 * adc081C027 light sensor driver
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
//#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/sysctl.h>
#include <linux/fs.h>

#include <linux/kobject.h>
#include <linux/device.h>

#define ADC_I2C_RATE     (188.9*1000)
#define ADC_SLAVE_ADDR   0x50

#define SENSOR_ON   1
#define SENSOR_OFF  0
#define LIGHTSENSOR_IOCTL_MAGIC 'l'
#define LIGHTSENSOR_IOCTL_GET_ENABLED    _IOR(LIGHTSENSOR_IOCTL_MAGIC, 1, int *)                                  
#define LIGHTSENSOR_IOCTL_ENABLE     _IOW(LIGHTSENSOR_IOCTL_MAGIC, 2, int *) 

#define DEBUG   1
 
#if DEBUG
#define DBG(X...)   printk(KERN_NOTICE X)
#else
#define DBG(X...)
#endif

struct i2c_client adc_client;

struct adc081C027_data {
    struct i2c_client   *client;
    struct timer_list   timer;
    struct work_struct  timer_work;
    struct input_dev    *input;
    int power_pin;
    int status;
};

static struct adc081C027_data *glight;

static int adc_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int adc_release(struct inode *inode, struct file *file)
{
    return 0;
}

static void adc_value_report(struct input_dev *input, int data)
{              
    unsigned char index = 0;
    if(data <= 10){
        index = 0;goto report;
    }          
    else if(data <= 160){
        index = 1;goto report;
    }          
    else if(data <= 225){
        index = 2;goto report;
    }          
    else if(data <= 320){
        index = 3;goto report;
    }          
    else if(data <= 640){
        index = 4;goto report;
    }          
    else if(data <= 1280){
        index = 5;goto report;
    }          
    else if(data <= 2600){
        index = 6;goto report;
    }          
    else{      
        index = 7;goto report;
    }          

report:        
    input_report_abs(input, ABS_MISC, index);
    input_sync(input);
    return;    
}

static int adc_i2c_reg_read(struct adc081C027_data *adc, const char *reg, char *buf, int count)
{
    int ret;
    if(count == 1 || count == 2)
    {    
        struct i2c_msg msgs[] = {
            {
                .addr   = adc->client->addr,
                .flags  = 0,
                .len    = 1,
                .buf    = (char *)reg,
            },
            {
                .addr   = adc->client->addr,
                .flags  = 1,
                .len    = count,
                .buf    = buf,
            },
        };
        ret = i2c_transfer(adc->client->adapter, msgs, 2);
    } else
        return -1;

    return (ret == 2) ? 2 : ret;
}

static int adc_start(struct adc081C027_data *adc)
{
    
    char buf[2];
    buf[0] = 0x02;
    buf[1] = 0xc0;
    int ret = i2c_master_send(adc->client, buf, 1);

    adc->status = SENSOR_ON; //sensor on

    adc->timer.expires = jiffies + 1*HZ;
    add_timer(&adc->timer);
    
    return 0;
}

static int adc_stop(struct adc081C027_data *data)
{
    struct adc081C027_data *adc = data;
    if(adc->status == 0)
        return 0;
    
    char buf[2];
    buf[0] = 0x02;
    buf[1] = 0x00;
    int ret = i2c_master_send(adc->client, buf, 1);
    ret = i2c_master_recv(adc->client, buf, 1);

    adc->status = SENSOR_OFF;
    del_timer(&adc->timer);
    return 0;
}

static int adc_read(struct i2c_client *client)
{   
    struct i2c_msg msg1, msg2;
    int ret;
    int result = 0;
    u8 reg, data[2];
    reg = 0x00;
    
    //write
    msg1.addr = ADC_SLAVE_ADDR;
    msg1.flags = 0;
    msg1.len = 1;
    msg1.buf = &reg;
    ret = i2c_transfer(client->adapter, &msg1, 1);
    //read
    msg2.addr = ADC_SLAVE_ADDR;
    msg2.flags = 1;
    msg2.len = 2;
    msg2.buf = data;
    ret = i2c_transfer(client->adapter, &msg2, 1);
    
    result = ( (data[0] << 8) | data[1] ) & 0xffff;
    adc_value_report(glight->input, result);

    return result;
}

static void adc_timer(unsigned long data)
{
    struct adc081C027_data *adc = (struct adc081C027_data *)data;
    schedule_work(&adc->timer_work);
}

static void adc_timer_work(struct work_struct *work)
{
    char result = 0;
    struct adc081C027_data *adc = container_of(work, struct adc081C027_data, timer_work);//don't know

    adc_read(glight->client);
    adc->timer.expires = jiffies + 1*HZ;
    add_timer(&adc->timer);
}

static long adc_ioctl( struct file *file, unsigned int cmd, unsigned long arg )
{
    unsigned int *argp = (unsigned int *)arg;
    struct adc081C027_data *adc = glight;
    switch(cmd) {
        case LIGHTSENSOR_IOCTL_GET_ENABLED:
            *argp = adc->status;
            break;
        case LIGHTSENSOR_IOCTL_ENABLE:
            if(*argp)
                adc_start(adc);
            else
                adc_stop(adc);
            break;
        default:break;
    }
    return 0;
}

static struct file_operations adc_fops = {
    .owner = THIS_MODULE,
    .open = adc_open,
    .release = adc_release,
    .unlocked_ioctl = adc_ioctl,
};

static struct miscdevice adc_device = {                                                                        
    .minor = MISC_DYNAMIC_MINOR,
    .name = "lightsensor",
    .fops = &adc_fops,
};

/*sysfs start*/
static ssize_t adc_attr_show_lux(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    int ret = sprintf(buf, "%d\n", adc_read(client));
    return ret; //if ret=0 lux node has the right value 
}

static DEVICE_ATTR(enable, 0777, NULL, NULL);
static DEVICE_ATTR(poll, 0777, NULL, NULL);
static DEVICE_ATTR(lux, 0777, adc_attr_show_lux, NULL);

static struct attribute *adc_sysfs[] = {
    &dev_attr_enable.attr,
    &dev_attr_poll.attr,
    &dev_attr_lux.attr,
    NULL
};

static struct attribute_group adc081C027_sysfs = {
    .attrs = adc_sysfs
};

/*sysfs end*/

static int adc081C027_probe(struct i2c_client *client,const struct i2c_device_id *id)
{	
    struct adc081C027_data *adc;
    int ret, err;

    /*adc init*/
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        dev_err(&client->dev, "Must have I2C_FUNC_I2C.\n");
        err = -ENODEV;
        goto alloc_memory_fail;
    }

    adc = kmalloc(sizeof(struct adc081C027_data), GFP_KERNEL);
    if(!adc) {
        printk("adc081C027 alloc memory err!!!\n");
        err = -ENOMEM;
        goto alloc_memory_fail;
    }

    adc->client = client;
    i2c_set_clientdata(client, adc);
    adc->status = 0;
    glight = adc;
    
    /*sysfs*/
    err = sysfs_create_group(&client->dev.kobj, &adc081C027_sysfs);
    if(err) {
        printk(KERN_ERR"adc : Unable to register sysfs: \n");
        goto exit_input_allocate_device_failed;
    }

    /*input dev*/
    adc->input = input_allocate_device();
    if(!adc->input) {
        err = -ENOMEM;
        printk(KERN_ERR"adc:Faild to allocate input device\n");
        goto exit_input_allocate_device_failed;
    }
    set_bit(EV_ABS, adc->input->evbit);
    input_set_abs_params(adc->input, ABS_MISC, 0, 10, 0, 0);
    adc->input->id.bustype = BUS_I2C;
    adc->input->name = "lightsensor-level";
    err = input_register_device(adc->input);
    if(err < 0) {
        printk(KERN_ERR"adc : Unable to register input deivce: %s\n",adc->input->name);
        goto exit_input_register_device_failed;
    }
   
    /*timer work*/
    INIT_WORK(&adc->timer_work, adc_timer_work);
    setup_timer(&adc->timer, adc_timer, (unsigned long)adc);
    /*misc*/
    err = misc_register(&adc_device);                                                                          
    if (err < 0) {
        printk(KERN_ERR"adc_probe: lightsensor_device register failed\n");
        goto exit_misc_register_fail;
    }
    adc_start(adc);

    return 0;

alloc_memory_fail:
    printk("%s error\n",__FUNCTION__);
    return err;
exit_input_allocate_device_failed:
    kfree(adc);
exit_input_register_device_failed:
    input_free_device(adc->input);
exit_misc_register_fail:
    input_unregister_device(adc->input);
}

/*
 * adc081C027_remove() - remove device
 * @client: I2C client device
 */
static int adc081C027_remove(struct i2c_client *client)
{	
    struct adc081C027_data *adc = i2c_get_clientdata(client);//for kfree
    printk("\nrdc081C027-----remove,exit!\n");
    kfree(adc);
    input_free_device(adc->input);
    input_unregister_device(adc->input);
    misc_deregister(&adc_device);
    return 0;
}

/* Device ID table */
static const struct i2c_device_id adc081C027_id[] = {
    { "adc081C027", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, adc081C027_id);
static struct i2c_driver adc081C027_driver = {
    .driver.name = "adc081C027",
    .probe       = adc081C027_probe,
    .remove      = adc081C027_remove,
    .id_table    = adc081C027_id,
};

static int __init adc081C027_init(void)
{  
    return i2c_add_driver(&adc081C027_driver);
}  

static void __exit adc081C027_exit(void)
{  
    i2c_del_driver(&adc081C027_driver);
}

module_init(adc081C027_init);
module_exit(adc081C027_exit);
MODULE_LICENSE("GPL");




