# What is flink?
flink enables any processors to communicate with an external FPGA over a serial interface. More information about the project can be found in [flink](http://flink-project.ch/start).

## flinkLinux
flink Kernel Modules for Linux offer drivers capabilities to communicate with various hardware interfaces. Among them are PCI and SPI. 
This documentation is intended for developers to understand the internals of the core driver. This facilitates the creation of new modules for other hardware interfaces. The user documentation for flinkLinux can be found in [flink](http://flink-project.ch/flink_linux).

## flink_axi
The bus module obtains its configuration data from the device tree. Alternatively, you can use hard-coded parameters by uncommenting the line "//#define CONFIG_SETTINGS_HARD_CODED" and adjusting the defines below. The device tree node should follow this structure:

```device-tree
flink_axi_0: flink_axi_registers@7aa00000 {
    compatible = "ost,flink-axi-1.0";   // do not change
    interrupt-parent = <&axi_intc_0>;   // Set this to the name of the IRQ controller used in the FPGA.
    interrupts = <0 0>;                 // do not change
    reg = <0x7aa00000 0xa000>;          // <[Base address] [length]>
    ost,flink-signal-offset = <34>;     // Signal offset to first signal nr
    ost,flink-nof-irq = <30>;           // Number of irq's
};
```
This node is tested with kernel 5.15.19-rt29-xilinx-v2022.1 --> Kernelversion 5.15.19 with the realtime patch.
Flink uses a lot of signals. Be careful with other kernels. It uses signals from (SIGRTMIN +2) up to SIGRTMAX. SIGRTMIN and (SIGRTMIN +1) has problems and should not be used!!!

## Documentation
- [Overview](doc/overview.md)
- [Bus Communication Modules](doc/bcm.md)
- [API](http://api.flink-project.ch/doc/flinklinux/html)
