#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include </usr/include/libusb-1.0/libusb.h>
#include <err.h>
#include <time.h>

#define VENDOR_INTERFACE 1
uint16_t vendor_id = 0x1B1C;
uint16_t product_id = 0x1BAF;

static void print_endpoint_comp(const struct libusb_ss_endpoint_companion_descriptor *ep_comp)
{
	printf("      USB 3.0 Endpoint Companion:\n");
	printf("        bMaxBurst:           %u\n", ep_comp->bMaxBurst);
	printf("        bmAttributes:        %02xh\n", ep_comp->bmAttributes);
	printf("        wBytesPerInterval:   %u\n", ep_comp->wBytesPerInterval);
}

static void print_endpoint(const struct libusb_endpoint_descriptor *endpoint)
{
	int i, ret;

	printf("      Endpoint:\n");
	printf("        bEndpointAddress:    %02xh\n", endpoint->bEndpointAddress);
	printf("        bmAttributes:        %02xh\n", endpoint->bmAttributes);
	printf("        wMaxPacketSize:      %u\n", endpoint->wMaxPacketSize);
	printf("        bInterval:           %u\n", endpoint->bInterval);
	printf("        bRefresh:            %u\n", endpoint->bRefresh);
	printf("        bSynchAddress:       %u\n", endpoint->bSynchAddress);

	for (i = 0; i < endpoint->extra_length;) {
		if (LIBUSB_DT_SS_ENDPOINT_COMPANION == endpoint->extra[i + 1]) {
			struct libusb_ss_endpoint_companion_descriptor *ep_comp;

			ret = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
			if (LIBUSB_SUCCESS != ret)
				continue;

			print_endpoint_comp(ep_comp);

			libusb_free_ss_endpoint_companion_descriptor(ep_comp);
		}

		i += endpoint->extra[i];
	}
}

static void print_altsetting(const struct libusb_interface_descriptor *interface)
{
	uint8_t i;

	printf("    Interface:\n");
	printf("      bInterfaceNumber:      %u\n", interface->bInterfaceNumber);
	printf("      bAlternateSetting:     %u\n", interface->bAlternateSetting);
	printf("      bNumEndpoints:         %u\n", interface->bNumEndpoints);
	printf("      bInterfaceClass:       %u\n", interface->bInterfaceClass);
	printf("      bInterfaceSubClass:    %u\n", interface->bInterfaceSubClass);
	printf("      bInterfaceProtocol:    %u\n", interface->bInterfaceProtocol);
	printf("      iInterface:            %u\n", interface->iInterface);

	for (i = 0; i < interface->bNumEndpoints; i++)
		print_endpoint(&interface->endpoint[i]);
}

static void print_2_0_ext_cap(struct libusb_usb_2_0_extension_descriptor *usb_2_0_ext_cap)
{
	printf("    USB 2.0 Extension Capabilities:\n");
	printf("      bDevCapabilityType:    %u\n", usb_2_0_ext_cap->bDevCapabilityType);
	printf("      bmAttributes:          %08xh\n", usb_2_0_ext_cap->bmAttributes);
}

static void print_ss_usb_cap(struct libusb_ss_usb_device_capability_descriptor *ss_usb_cap)
{
	printf("    USB 3.0 Capabilities:\n");
	printf("      bDevCapabilityType:    %u\n", ss_usb_cap->bDevCapabilityType);
	printf("      bmAttributes:          %02xh\n", ss_usb_cap->bmAttributes);
	printf("      wSpeedSupported:       %u\n", ss_usb_cap->wSpeedSupported);
	printf("      bFunctionalitySupport: %u\n", ss_usb_cap->bFunctionalitySupport);
	printf("      bU1devExitLat:         %u\n", ss_usb_cap->bU1DevExitLat);
	printf("      bU2devExitLat:         %u\n", ss_usb_cap->bU2DevExitLat);
}

static void print_interface(const struct libusb_interface *interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
		print_altsetting(&interface->altsetting[i]);
}

static void print_bos(libusb_device_handle *handle)
{
	struct libusb_bos_descriptor *bos;
	uint8_t i;
	int ret;

	ret = libusb_get_bos_descriptor(handle, &bos);
	if (ret < 0)
		return;

	printf("  Binary Object Store (BOS):\n");
	printf("    wTotalLength:            %u\n", bos->wTotalLength);
	printf("    bNumDeviceCaps:          %u\n", bos->bNumDeviceCaps);

	for (i = 0; i < bos->bNumDeviceCaps; i++) {
		struct libusb_bos_dev_capability_descriptor *dev_cap = bos->dev_capability[i];

		if (dev_cap->bDevCapabilityType == LIBUSB_BT_USB_2_0_EXTENSION) {
			struct libusb_usb_2_0_extension_descriptor *usb_2_0_extension;

			ret = libusb_get_usb_2_0_extension_descriptor(NULL, dev_cap, &usb_2_0_extension);
			if (ret < 0)
				return;

			print_2_0_ext_cap(usb_2_0_extension);
			libusb_free_usb_2_0_extension_descriptor(usb_2_0_extension);
		} else if (dev_cap->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {
			struct libusb_ss_usb_device_capability_descriptor *ss_dev_cap;

			ret = libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_dev_cap);
			if (ret < 0)
				return;

			print_ss_usb_cap(ss_dev_cap);
			libusb_free_ss_usb_device_capability_descriptor(ss_dev_cap);
		}
	}

	libusb_free_bos_descriptor(bos);
}

static void print_configuration(struct libusb_config_descriptor *config)
{
	uint8_t i;

	printf("  Configuration:\n");
	printf("    wTotalLength:            %u\n", config->wTotalLength);
	printf("    bNumInterfaces:          %u\n", config->bNumInterfaces);
	printf("    bConfigurationValue:     %u\n", config->bConfigurationValue);
	printf("    iConfiguration:          %u\n", config->iConfiguration);
	printf("    bmAttributes:            %02xh\n", config->bmAttributes);
	printf("    MaxPower:                %u\n", config->MaxPower);

	for (i = 0; i < config->bNumInterfaces; i++)
		print_interface(&config->interface[i]);
}

static void print_device(libusb_device *dev, libusb_device_handle *handle, int verbose)
{
	struct libusb_device_descriptor desc;
	unsigned char string[256];
	const char *speed;
	int ret;
	uint8_t i;

	switch (libusb_get_device_speed(dev)) {
	case LIBUSB_SPEED_LOW:		speed = "1.5M"; break;
	case LIBUSB_SPEED_FULL:		speed = "12M"; break;
	case LIBUSB_SPEED_HIGH:		speed = "480M"; break;
	case LIBUSB_SPEED_SUPER:	speed = "5G"; break;
	case LIBUSB_SPEED_SUPER_PLUS:	speed = "10G"; break;
	default:			speed = "Unknown";
	}

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return;
	}

	printf("Dev (bus %u, device %u): %04X - %04X speed: %s\n",
	       libusb_get_bus_number(dev), libusb_get_device_address(dev),
	       desc.idVendor, desc.idProduct, speed);

	if (!handle)
		libusb_open(dev, &handle);

	if (handle) {
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				printf("  Manufacturer:              %s\n", (char *)string);
		}

		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				printf("  Product:                   %s\n", (char *)string);
		}

		if (desc.iSerialNumber && verbose) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
			if (ret > 0)
				printf("  Serial Number:             %s\n", (char *)string);
		}
	}

	if (verbose) {
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;

			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS != ret) {
				printf("  Couldn't retrieve descriptors\n");
				continue;
			}

			print_configuration(config);

			libusb_free_config_descriptor(config);
		}

		if (handle && desc.bcdUSB >= 0x0201)
			print_bos(handle);
	}

	if (handle)
		libusb_close(handle);
}

int main(void) {
    int res = libusb_init(NULL); // NULL is the default libusb_context

    if (res != LIBUSB_SUCCESS) {
        fprintf(stdout,"\n\nERROR: Cannot Initialize libusb\n\n");
        goto handleError;
    }

    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
    // libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_NONE);

    struct libusb_device_handle *dh = NULL; // The device handle
    dh = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);

    libusb_device *dev = libusb_get_device(dh);
    // print_device(dev, dh, 1);
    res = libusb_get_device_speed(dev);
    if ((res < 0) || (res > 5)) {
        res = 0;
    }
    const char* const speed_name[6] = { "Unknown", "1.5 Mbit/s (USB LowSpeed)", "12 Mbit/s (USB FullSpeed)",
    "480 Mbit/s (USB HighSpeed)", "5000 Mbit/s (USB SuperSpeed)", "10000 Mbit/s (USB SuperSpeedPlus)" };
	printf("\n\nDevice speed: %s\n", speed_name[res]);

    if (dh == NULL) {
        fprintf(stdout,"\n\nERROR: Cannot connect to device %x\n\n", product_id);
        goto handleError;
    }

    res = libusb_set_auto_detach_kernel_driver(dh, 1);
    if (res != LIBUSB_SUCCESS) {
        fprintf(stdout,"\n\nERROR: auto_detach_kernel_driver Error\n\n");
        goto handleError;
    }

    // libusb_reset_device(dh);
    // if (res != LIBUSB_SUCCESS) {
    //     fprintf(stdout,"\n\nERROR: reset_device Error\n\n");
    //     goto handleError;
    // }

    if (libusb_kernel_driver_active(dh, VENDOR_INTERFACE) == 1) {
        printf("Kernel Driver Active\n");
        // if (libusb_detach_kernel_driver(dh, 0) == LIBUSB_SUCCESS) {
        //     printf("Kernel Driver Detached!\n");
        // } else {
        //     fprintf(stdout, "\n\nKernel Driver Detach Failed!\n\n");
        //     goto handleError;
        // }
    }

    printf("\nClaiming interface %d...\n", VENDOR_INTERFACE);
    res = libusb_claim_interface(dh, VENDOR_INTERFACE);
    if (res != LIBUSB_SUCCESS) {
        fprintf(stdout,"\n\n Cannot Claim Interface!\n\n");
        goto handleError;
    }

    // libusb_set_configuration(dh, 0);
    // if (res != LIBUSB_SUCCESS) {
    //     fprintf(stdout,"\n\nERROR: set_configuration Error\n\n");
    //     goto handleError;
    // }

    // uint8_t pingCmd[] = {0x00, 0x08, 0x12, 0x00, 0x00, 0x00};
    // uint8_t hostControlCmd[] = {0x00, 0x08, 0x01, 0x03, 0x00, 0x02};
    // uint8_t blue[] = {0x00, 0x08, 0x01, 0x04, 0x00, 0x03, 0x00};
    // uint8_t reset[] = {0x00, 0x08, 0x0F, 0x00, 0x00, 0x00};

    uint8_t pingCmd[] = {0x08, 0x12, 0x00, 0x00, 0x00};
    uint8_t hostControlCmd[] = {0x08, 0x01, 0x03, 0x00, 0x02};
    uint8_t blue[] = {0x08, 0x01, 0x04, 0x00, 0x03, 0x00};
    uint8_t reset[] = {0x08, 0x0F, 0x00, 0x00, 0x00};

    clock_t prev_time = 0;
    clock_t period = 1000;
    while (1) {
        clock_t current_time = clock() / 1000;
        
        int len = 0;
        if (current_time - prev_time > period) {
            prev_time = current_time;
            res = libusb_interrupt_transfer(dh, 1, pingCmd, sizeof(pingCmd), &len, 10);
            if (res != LIBUSB_SUCCESS) {
                printf("res = %d, len = %d\n", res, len);
                fprintf(stdout,"\n\nERROR: Transfer pingCmd Error\n\n");
                goto handleError;
            }

            res = libusb_interrupt_transfer(dh, 1, hostControlCmd, sizeof(hostControlCmd), &len, 10);
            if (res != LIBUSB_SUCCESS) {
                printf("res = %d, len = %d\n", res, len);
                fprintf(stdout,"\n\nERROR: Transfer hostControlCmd Error\n\n");
                goto handleError;
            }
        }

        // len = 0;
        // res = libusb_interrupt_transfer(dh, 1, blue, sizeof(blue), &len, 10);
        // if (res != LIBUSB_SUCCESS) {
        //     printf("res = %d, len = %d\n", res, len);
        //     fprintf(stdout,"\n\nERROR: Transfer blue cmd %d Error\n\n", i);
        //     goto handleError;
        // }

        // len = 0;
        // res = libusb_interrupt_transfer(dh, 1, reset, sizeof(reset), &len, 10);
        // if (res != LIBUSB_SUCCESS) {
        //     printf("res = %d, len = %d\n", res, len);
        //     fprintf(stdout,"\n\nERROR: Transfer reset cmd %d Error\n\n", i);
        //     goto handleError;
        // }
    }

    // res = libusb_release_interface(dh, VENDOR_INTERFACE);
    // if(res != LIBUSB_SUCCESS) {
    //     fprintf(1,"\n\nCannot Release Interface\n\n");
    //     goto handleError;
    // }

    goto handleError;

    handleError:
    libusb_release_interface(dh, VENDOR_INTERFACE);
    libusb_attach_kernel_driver(dh, VENDOR_INTERFACE);
    libusb_close(dh);
    libusb_exit(NULL);
    return 0;
}

// gcc -g script.c -L/usr/local/lib -lusb-1.0 -I/usr/local/include -o cScript
