#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>

#define EDU_ID 0x00        // RO
#define EDU_LIVE 0x04      // rw
#define EDU_FACTORIAL 0x08 // rw
#define EDU_FACT_STAT 0x20 // rw

struct mydev_priv {
  void __iomem *mmio;
  struct cdev cdev;
  dev_t devno;
  struct class *edu_class;
};

static const struct pci_device_id mydev_ids[] = {{PCI_DEVICE(0x1234, 0x11e8)},
                                                 {0}

};
MODULE_DEVICE_TABLE(pci, mydev_ids);

static int edu_open(struct inode *inode, struct file *file) {
  printk("edu: Device open\n");
  return 0;
}

static int edu_release(struct inode *inode, struct file *file) {
  printk("edu: Device close\n");
  return 0;
}

static long edu_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  printk("edu: Device ioctl\n");
  return 0;
}

static ssize_t edu_read(struct file *file, char __user *buf, size_t count,
                        loff_t *offset) {
  printk("edu: Device read\n");
  return 0;
}

static ssize_t edu_write(struct file *file, const char __user *buf,
                         size_t count, loff_t *offset) {
  printk("edu: Device write\n");
  return 0;
}

static const struct file_operations cdev_fops = {.owner = THIS_MODULE,
                                                 .open = edu_open,
                                                 .release = edu_release,
                                                 .unlocked_ioctl = edu_ioctl,
                                                 .read = edu_read,
                                                 .write = edu_write};

static int mydev_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
  struct mydev_priv *priv;
  struct device *edu_device;
  u32 val;
  int ret;

  dev_info(&pdev->dev, "edu device probed\n");

  // Enable the PCI device? I still get exactly what this enables
  ret = pci_enable_device(pdev);
  if (ret) {
    dev_err(&pdev->dev, "cannot enable PCI device\n");
    return ret;
  }

  // Mark all PCI regions associated with this pdev as reserved "mydev"
  ret = pci_request_regions(pdev, "mydev");
  if (ret) {
    dev_err(&pdev->dev, "failed to get regions\n");
    goto err_disable;
  }

  // Allocate memory heap memory the size of a pointer to contain the pointer to
  // the mmio region
  priv = kzalloc(sizeof(*priv), GFP_KERNEL);
  if (!priv) {
    ret = -ENOMEM;
    goto err_release;
  }

  // Returns a __iomem pointer to the device bar
  priv->mmio = pci_iomap(pdev, 0, 0);
  if (!priv->mmio) {
    dev_err(&pdev->dev, "failed to map BAR 0\n");
    ret = -ENOMEM;
    goto err_free;
  }

  // Set a private data pointer for a pci_dev
  pci_set_drvdata(pdev, priv);

  ret = alloc_chrdev_region(&priv->devno, 0, 1, "edu");
  if (ret) {
    dev_err(&pdev->dev, "Failed allocate edu cdev");
    goto err_unmap;
  }

  cdev_init(&priv->cdev, &cdev_fops);
  priv->cdev.owner = THIS_MODULE;

  ret = cdev_add(&priv->cdev, priv->devno, 1);
  if (ret) {
    dev_err(&pdev->dev, "failed to add cdev\n");
    goto err_unregister;
  }

  priv->edu_class = class_create("edu_dev");
  if (IS_ERR(priv->edu_class)) {
    dev_err(&pdev->dev, "failed to create cdev class\n");
    ret = PTR_ERR(priv->edu_class);
    goto err_cdev_del;
  }

  edu_device =
      device_create(priv->edu_class, NULL, priv->devno, NULL, "edu_dev-%d", 1);
  if (IS_ERR(edu_device)) {
    dev_err(&pdev->dev, "failed to create device node\n");
    ret = PTR_ERR(edu_device);
    goto err_class_destroy;
  }

  val = ioread32(priv->mmio + EDU_ID);
  dev_info(&pdev->dev, "identification: 0x%08x\n", val);

  iowrite32(0x05, priv->mmio + EDU_FACTORIAL);

  while (1) {
    u32 status = ioread32(priv->mmio + EDU_FACT_STAT);

    if (status & 1) {
      continue;
    } else {
      break;
    }
  }

  val = ioread32(priv->mmio + EDU_FACTORIAL);
  printk("Facotial result: %d\n", val);

  return 0;

err_class_destroy:
  class_destroy(priv->edu_class);
err_cdev_del:
  cdev_del(&priv->cdev);
err_unregister:
  unregister_chrdev_region(priv->devno, 1);
err_unmap:
  pci_iounmap(pdev, priv->mmio);
err_free:
  kfree(priv);
err_release:
  pci_release_regions(pdev);
err_disable:
  pci_disable_device(pdev);
  return ret;
}

static void mydev_remove(struct pci_dev *pdev) {
  struct mydev_priv *priv = pci_get_drvdata(pdev);

  if (priv) {
    device_destroy(priv->edu_class, priv->devno);
    class_destroy(priv->edu_class);
    cdev_del(&priv->cdev);
    unregister_chrdev_region(priv->devno, 1);
    pci_iounmap(pdev, priv->mmio);
    kfree(priv);
  }
  pci_release_regions(pdev);
  pci_disable_device(pdev);
  dev_info(&pdev->dev, "edu device removed\n");
}

static struct pci_driver mydev_driver = {
    .name = "mydev",
    .id_table = mydev_ids,
    .probe = mydev_probe,   /* called when device found */
    .remove = mydev_remove, /* called on device removal */
};

static int __init hello_init(void) {
  pr_info("Hello, world\n");

  return pci_register_driver(&mydev_driver);
}

static void __exit hello_exit(void) {
  pr_info("Goodbye, world\n");
  pci_unregister_driver(&mydev_driver);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");