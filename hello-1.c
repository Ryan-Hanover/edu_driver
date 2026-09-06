#include "asm-generic/iomap.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>


struct mydev_priv {
    void __iomem *mmio;
};

static const struct pci_device_id mydev_ids[] = {
    { PCI_DEVICE(0x1234, 0x11e8) },
    {0}  

};
MODULE_DEVICE_TABLE(pci, mydev_ids);

static int mydev_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct mydev_priv *priv;
    u32 val;
    int ret;

    dev_info(&pdev->dev, "edu device probed\n");

    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "cannot enable PCI device\n");
        return ret;
    }

    ret = pci_request_regions(pdev, "mydev");
    if (ret) {
        dev_err(&pdev->dev, "failed to get regions\n");
        goto err_disable;
    }

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        ret = -ENOMEM;
        goto err_release;
    }

    priv->mmio = pci_iomap(pdev, 0, 0);
    if (!priv->mmio) {
        dev_err(&pdev->dev, "failed to map BAR 0\n");
        ret = -ENOMEM;
        goto err_free;
    }

    pci_set_drvdata(pdev, priv);

    val = ioread32(priv->mmio);
    dev_info(&pdev->dev, "identification: 0x%08x\n", val);

    iowrite16(0x03, priv->mmio + 0x08);
    iowrite16(0x0, priv->mmio+0x20);

    val = ioread32(priv->mmio + 0x08);
    printk("Facotial result: %d\n", val);
    
    return 0;

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

    dev_info(&pdev->dev, "edu device removed\n");

    if (priv) {
        pci_iounmap(pdev, priv->mmio);
        kfree(priv);
    }
    pci_release_regions(pdev);
    pci_disable_device(pdev);
}

static struct pci_driver mydev_driver = {
    .name     = "mydev",
    .id_table = mydev_ids,
    .probe    = mydev_probe,    /* called when device found */
    .remove   = mydev_remove,   /* called on device removal */
};


static int __init hello_init(void)
{
	pr_info("Hello, world\n");
    pci_register_driver(&mydev_driver);

	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("Goodbye, world\n");
    pci_unregister_driver(&mydev_driver);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");