
#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif



#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>



/* usb headers */
#include <linux/input.h>
#include <linux/usb.h>



/* miscdevice headers */
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>



/*
 * Version Information
 */
#define DRIVER_VERSION "v0.1"   //version of device
#define DRIVER_AUTHOR "Poojitha Bhagya Sneha Priyanka "
#define DRIVER_DESC "Mouse driver"
#define DRIVER_LICENSE "GPL"



MODULE_AUTHOR(DRIVER_AUTHOR);  //declares module’s author
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);



#define DEVICE_NAME "mousek"
#define MAX_SCREEN 100
#define MIN_SCREEN 0



int Major; 



 /*
 * We need a local data structure, as it must be allocated for each new
 * mouse device plugged in the USB bus
 */



struct mousek_device {
    signed char data[4];     /* use a 4-byte protocol */
    struct urb urb;          /* USB Request block, to get USB data*/
    struct input_dev *idev;   /* input device, to push out input  data */
    int x, y;                /* keep track of the position of this device */
};



static struct mousek_device *mouse;



/*
 * Handler for data sent in by the device. The function is called by
 * the USB kernel subsystem whenever the device spits out new data
 */



int mousek_open(struct inode *inode, struct file *filp)
{
    



    /* announce yourself */
/*This is called while giving input which involves writing the instruction into the mousek file*/
    printk(KERN_INFO "mousek: faking an USB mouse via the misc device\n");
    
    
    return 0; /* Ok */
}    



/* close releases the device, like mousek_disconnect */
int mousek_release(struct inode *inode, struct file *filp)
{

    /*this function erases the data associated with the device which is assigned during command execution. But device is not unregisterd*/


    struct mousek_device *mousek = filp->private_data;
    input_unregister_device(mouse->idev);
    kfree(mousek); //to free kernel memory of mousek
    usb_kill_urb(&mouse->urb); //to kill the current execution



    printk(KERN_INFO "mousek: closing misc device\n");
    
    
    /*
    * Decrement the usage count, or else once you opened the file, you'll
    * never get rid of the module.
    */
    module_put(THIS_MODULE); //decrements module usage count
    
    return 0;
}



/* poll reports the device as writeable */
ssize_t mousek_read(struct file *filp,   /* see include/linux/fs.h   */
                           char *buffer,        /* buffer to fill with data */
                           size_t length,       /* length of the buffer     */
                           loff_t * offset)
{
    printk(KERN_INFO "mousek: Restricted. That is okay but do not repeat this mistake.\n");
    return 0;
}



/* write accepts data and converts it to mouse movement
 * Called when a process writes to dev file: echo "hi" > /dev/hello
 */
ssize_t mousek_write(struct file *filp, const char *buf, size_t count,
            loff_t *offp)
{
    struct mousek_device *mouse = filp->private_data;
    static char localbuf[16];
    struct urb urb;
    int i, command = -1;



    /* accept 16 bytes at a time, at most */
    if (count >16) count=16;
    copy_from_user(localbuf, buf, count);



    /* prepare the urb for mousek_irq() */
    urb.status = USB_ST_NOERROR;
    urb.context = mouse;
    struct input_dev *dev = mouse->idev;
    
    //first character will tell what command to do
    //i : instruction sequence

    switch(localbuf[0]){
        case 'i': case 'I':{   /*It will be followed by sequence of instructions. To store execute them first separate cases using command variable*/
            command = 0;
            break;
        }
        default : break;
    }
    
    if(command == 0){  //if first character is ‘i’
        /* scan written sequence */
        for (i=2; i<count; i++) {
            mouse->data[1] = mouse->data[2] = mouse->data[3] = 0;
            
            switch (localbuf[i]) 
            {  /*checking what next character is, to find what next instruction will be*/
                    
                case 'q': /* left click down */
                    mouse->data[3] = 1;
                    break;
                case 'Q': /* left click up */
                    mouse->data[3] = 2;
                    break;
                case 'w': /* right click down */
                    mouse->data[3] = 3;
                    break;
                case 'W': /* right click up */
                    mouse->data[3] = 4;
                    break;
                default:
                    i = count;
                continue;

            }
            
            input_report_key(dev, KEY_UP, 1); /* keypress */
            input_report_key(dev, KEY_UP, 0); /* release */
            
            
            if(mouse->data[1] != 0){
                input_report_rel(dev, REL_X, mouse->data[1]);
                
                input_sync(dev); //input events are reported to the userspace immediately



            }
            if(mouse->data[2] != 0){
                input_report_rel(dev, REL_Y, mouse->data[2]);
                
                input_sync(dev);



            }



            //handle queue clicks
            if(mouse->data[3] != 0){
                if(mouse->data[3] == 1){
                    input_report_key(dev, BTN_LEFT, 1);                    
                }else if(mouse->data[3] == 2){
                    input_report_key(dev, BTN_LEFT, 0);
                }else if(mouse->data[3] == 3){
                    input_report_key(dev, BTN_RIGHT, 1);
                }else if(mouse->data[3] == 4){
                    input_report_key(dev, BTN_RIGHT, 0);
                }
                
                input_sync(dev);
            } 
            
            
            input_report_rel(dev, REL_WHEEL, mouse->data[3]);
            
            /* if we are here, a valid key is there, fix the urb */
            mousek_irq(&urb);


        }//for
        
    }
    }
    

    //right click press down
    input_report_key(dev, BTN_RIGHT, 1);
    //right click up
    input_report_key(dev, BTN_RIGHT, 0);


    input_sync(dev);


    input_report_rel(mouse->idev, REL_WHEEL, _data[3]);
    
    return count;
}



//structure to manage function calls


struct file_operations mousek_fops = {
    write:    mousek_write,
    read:     mousek_read,
    open:     mousek_open,
    release:  mousek_release,
};



/*
 * Functions called at module load and unload time: only register and
 * unregister the USB callbacks and the misc entry point
 */

//Here is the main method where execution gets started
int init_module(void)
{
    int retval;
    
    Major = register_chrdev(0, DEVICE_NAME, &mousek_fops);
//Major number is number associated with device drivers
    if (Major < 0) {
      printk(KERN_ALERT "Registering char device failed with %d\n", Major);
      return Major;
    }
    
    
    struct input_dev *input_dev;



    /* allocate and zero a new data structure for the new device */
    mouse = kmalloc(sizeof(struct mousek_device), GFP_KERNEL);
    if (!mouse) return -ENOMEM; /* failure */
    memset(mouse, 0, sizeof(*mouse));



    input_dev = input_allocate_device();
    if (!input_dev) {
        printk(KERN_ERR "mousek.c: Not enough memory\n");
        retval = -ENOMEM;
        //goto err_free_irq;
    }
    //updating struct
    mouse->idev = input_dev;
    
    /* tell the features of this input device: fake only keys */
    //mouse->idev.evbit[0] = BIT(EV_KEY);
    /* and tell which keys: only the arrows */
    set_bit(103, mouse->idev->keybit); /* Up    */
    set_bit(105, mouse->idev->keybit); /* Left  */
    set_bit(106, mouse->idev->keybit); /* Right */
    set_bit(108, mouse->idev->keybit); /* Down  */


    /*Assigning the attributes of the data structure with the macro codes of code fragments necessary for execution*/
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    
    set_bit(103, input_dev->keybit); /* Up    */

    input_dev->name = DEVICE_NAME;    //setting device name
    input_set_drvdata(input_dev, mouse);
    
    retval = input_register_device(input_dev);
    if (retval) {
        printk(KERN_ERR "mousek: Failed to register device\n");
        goto err_free_dev;
    }

        printk(KERN_INFO "Assigned major number to this driver is %d.\n", Major);
	printk(KERN_INFO "To interact with the driver, create a dev file with\n");   
        printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Echo to the device file\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");
	;
    
    
return 0;



err_free_dev:
    input_free_device(mouse->idev);
    kfree(mouse);



return retval;
}



void cleanup_module(void)
{
    /*
    * Unregister the device
    */
    if(!mouse) return;
    
    input_unregister_device(mouse->idev);
    kfree(mouse);    //releasing kernel memory
    unregister_chrdev(Major, DEVICE_NAME); /*unregistering the device*/
    
    printk(KERN_ALERT "Uninstalled. Delete device from dev.");


}

