#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include </usr/include/libusb-1.0/libusb.h>
#include <err.h>
#include <time.h>

#define VENDOR_ID 0x1B1C
#define PRODUCT_ID 0x1BAF
#define VENDOR_INTERFACE 1
#define VENDOR_IN_ENDPOINT 0x82
#define VENDOR_OUT_ENDPOINT 0x01

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

typedef enum {
    СMD_READ_FW_VERSION,
    СMD_READ_BL_VERSION,
    СMD_PING,
    СMD_HOST_CONTROL,
    СMD_LED_CALIBRATION,
    СMD_COLOR_RED,
    СMD_COLOR_GREEN,
    СMD_COLOR_BLUE,
    СMD_COLOR_WHITE,
    СMD_COLOR_BLACK,
    СMD_RESET_TO_DEFAULTS,
    СMD_COLOR_COUNT,
} CMD;

static bool sendCommand(libusb_device_handle * const dev_handle, CMD cmd)
{
    uint8_t commands[][1024] = {
        [СMD_READ_FW_VERSION] =     {0x08, 0x02, 0x13, 0x00, 0x00},
        [СMD_READ_BL_VERSION] =     {0x08, 0x02, 0x14, 0x00, 0x00},
        [СMD_PING] =                {0x08, 0x12},
        [СMD_HOST_CONTROL] =        {0x08, 0x01, 0x03, 0x00, 0x02},
        [СMD_LED_CALIBRATION] =     {0x08,0x04,0x04,0x00,0x05,
                                    0x14,0x26,0x23,0x35,
                                    0x28,0x48,0x5A,0x66,
                                    0x3C,0x6C,0x99,0x99,
                                    0x50,0x90,0xCC,0xCC,
                                    0x64,0xB4,0xFF,0xFF},
        [СMD_COLOR_RED] =           {0x08, 0x01, 0x04, 0x00, 0x02, 0x00},
        [СMD_COLOR_GREEN] =         {0x08, 0x01, 0x04, 0x00, 0x03, 0x00},
        [СMD_COLOR_BLUE] =          {0x08, 0x01, 0x04, 0x00, 0x04, 0x00},
        [СMD_COLOR_WHITE] =         {0x08, 0x01, 0x04, 0x00, 0x05, 0x00},
        [СMD_COLOR_BLACK] =         {0x08, 0x01, 0x04, 0x00, 0x01, 0x00},
        [СMD_RESET_TO_DEFAULTS] =   {0x08, 0x0F, 0x00, 0x00, 0x00},
    };
	// uint8_t readVid[1024] = {0x08, 0x02, 0x11, 0x00};
    const char * cmdName[] = {
        "СMD_READ_FW_VERSION",
        "СMD_READ_BL_VERSION",
        "СMD_PING",
        "СMD_HOST_CONTROL",
        "СMD_LED_CALIBRATION",
        "СMD_COLOR_RED",
        "СMD_COLOR_GREEN",
        "СMD_COLOR_BLUE",
        "СMD_COLOR_WHITE",
        "СMD_COLOR_BLACK",
        "СMD_RESET_TO_DEFAULTS",
        "СMD_COLOR_COUNT",
    };

    assert(cmd < СMD_COLOR_COUNT);
    printf("Send command %s\n", cmdName[cmd]);
    int len = 0;
    int res = libusb_interrupt_transfer(dev_handle, VENDOR_OUT_ENDPOINT, commands[cmd], sizeof(commands[cmd]), &len, 10);
    
    if (res != LIBUSB_SUCCESS) {
        printf("res = %d, len = %d\n", res, len);
        fprintf(stdout,"\n\nERROR: Transfer %s Error\n\n", cmdName[cmd]);
    }

    uint8_t reply[1024] = {0};
    res = libusb_interrupt_transfer(dev_handle, VENDOR_IN_ENDPOINT, reply, sizeof(reply), &len, 100);
    if (res != LIBUSB_SUCCESS) {
        printf("res = %d, len = %d\n", res, len);
        fprintf(stdout,"\n\nERROR: reply %s Error\n\n", cmdName[cmd]);
    } else {
        // printf("reply res = %d, len = %d\n", res, len);
        // for (int i = 0; i < len; i++) {
        //     printf("%x ", reply[i]);
        // }
        // printf("\n");
        printf("Reply OK\n");
    }

    return res == LIBUSB_SUCCESS;
}

struct libusb_device_handle *dev_handle = NULL;

static libusb_device_handle * connect(uint16_t vid, uint16_t pid)
{
    printf("Establish connection...\n");

    struct libusb_device_handle *dh = NULL; // The device handle
    dh = libusb_open_device_with_vid_pid(NULL, vid, pid);
    if (dh == NULL) {
        fprintf(stdout,"\n\nERROR: libusb_open_device FAILED!\n\n");
        return NULL;
    }
    printf("dev_handle == %p \n", dh);

    libusb_device *dev = libusb_get_device(dh);
    // print_device(dev, dh, 1);
    int res = libusb_get_device_speed(dev);
    if ((res < 0) || (res > 5)) {
        res = 0;
    }
    const char* const speed_name[6] = { "Unknown", "1.5 Mbit/s (USB LowSpeed)", "12 Mbit/s (USB FullSpeed)",
    "480 Mbit/s (USB HighSpeed)", "5000 Mbit/s (USB SuperSpeed)", "10000 Mbit/s (USB SuperSpeedPlus)" };
	printf("\n\nDevice speed: %s\n", speed_name[res]);

    if (dh == NULL) {
        fprintf(stdout,"\n\nERROR: Cannot connect to device %x\n\n", pid);
        return NULL;
    }

    res = libusb_set_auto_detach_kernel_driver(dh, 1);
    if (res != LIBUSB_SUCCESS) {
        fprintf(stdout,"\n\nERROR: auto_detach_kernel_driver Error\n\n");
        return NULL;
    }

    // libusb_reset_device(dh);
    // if (res != LIBUSB_SUCCESS) {
    //     fprintf(stdout,"\n\nERROR: reset_device Error\n\n");
    //     return NULL;
    // }

    if (libusb_kernel_driver_active(dh, VENDOR_INTERFACE) == 1) {
        printf("Kernel Driver Active\n");
        if (libusb_detach_kernel_driver(dh, 0) == LIBUSB_SUCCESS) {
            printf("Kernel Driver Detached!\n");
        } else {
            fprintf(stdout, "\n\nKernel Driver Detach Failed!\n\n");
        }
    }

    res = libusb_claim_interface(dh, VENDOR_INTERFACE);
    if (res == LIBUSB_SUCCESS) {
        printf("Claim Interface OK!\n");
    } else {
        fprintf(stdout,"\n\n Cannot Claim Interface!\n\n");
    }

    // libusb_set_configuration(dh, 0);
    // if (res != LIBUSB_SUCCESS) {
    //     fprintf(stdout,"\n\nERROR: set_configuration Error\n\n");
    //     goto handleError;
    // }

    return dh;
}

int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void *user_data) {
    
    struct libusb_device_descriptor desc;
    int res;
    
    (void)libusb_get_device_descriptor(dev, &desc);

    if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
        printf("HOTPLUG_EVENT_DEVICE_ARRIVED\n");
        dev_handle = connect(VENDOR_ID, PRODUCT_ID);
        if (dev_handle == NULL) {
            libusb_exit(NULL);
            exit(1);
        }
    } else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        printf("HOTPLUG_EVENT_DEVICE_LEFT\n");
        if (dev_handle) {
            libusb_release_interface(dev_handle, VENDOR_INTERFACE);
            libusb_attach_kernel_driver(dev_handle, VENDOR_INTERFACE);
            libusb_close(dev_handle);
            dev_handle = NULL;
        }
    } else {
        printf("Unhandled event %d\n", event);
    }

    return 0;
}

int main(void) {
    int res = libusb_init(NULL); // NULL is the default libusb_context

    if (res != LIBUSB_SUCCESS) {
        fprintf(stdout,"\n\nERROR: Cannot Initialize libusb\n\n");
        libusb_exit(NULL);
        exit(1);
    }

    // libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    dev_handle = connect(VENDOR_ID, PRODUCT_ID);
    if (dev_handle == NULL) {
        libusb_exit(NULL);
        exit(1);
    }

    libusb_hotplug_callback_handle callback_handle;
    res = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                          LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, VENDOR_ID, PRODUCT_ID,
                                          LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL,
                                          &callback_handle);
    if (res != LIBUSB_SUCCESS) {
        fprintf(stdout, "\n\nError creating a hotplug callback!\n\n");
        libusb_exit(NULL);
        exit(1);
    }

    clock_t prev_time = 0;
    clock_t period = 100;
    CMD cmd = СMD_READ_FW_VERSION;

    printf("Start script\n");
    FILE *file = fopen("cnt.txt","w+");
    if (file == NULL) {
        fprintf(stdout, "\n\nCan't open a file!\n\n");
    }
    uint32_t runCounter = 0;
    while (1)
    {
        clock_t current_time = clock() / 1000;
    
        if (dev_handle != NULL && current_time - prev_time > period) {
            prev_time = current_time;
            period = 100;
            
            printf("Tick %ld ms :", current_time);
            bool res = sendCommand(dev_handle, cmd);
            if (cmd == СMD_RESET_TO_DEFAULTS) {
                period = 1500;
                runCounter++;
                fseek(file, 0, SEEK_SET);
                fprintf(file, "%u", runCounter);
            } else if (!res) {
                goto handleError;
            }
            cmd = (cmd + 1) % СMD_COLOR_COUNT;
        }
        struct timeval tv = {0 ,0};
        libusb_handle_events_timeout_completed(NULL, &tv, NULL);
    }

    handleError:
    libusb_hotplug_deregister_callback(NULL, callback_handle);
    libusb_release_interface(dev_handle, VENDOR_INTERFACE);
    libusb_attach_kernel_driver(dev_handle, VENDOR_INTERFACE);
    libusb_close(dev_handle);
    libusb_exit(NULL);
    fclose(file);
    return 0;
}

// gcc -g script.c -lusb-1.0 -o usbTest
// sudo ./usbTest
