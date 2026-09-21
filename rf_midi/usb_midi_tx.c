#include "usb_midi_tx.h"

#include <furi.h>
#include <usb.h>
#include <usb_std.h>

#define USB_MIDI_VID     0x6666
#define USB_MIDI_PID     0x5119
#define USB_MIDI_EP0     8
#define USB_MIDI_EP_IN   0x81
#define USB_MIDI_EP_SIZE 64

#define USB_AUDIO_CLASS            0x01
#define USB_AUDIO_SUBCLASS_CONTROL 0x01
#define USB_AUDIO_SUBCLASS_MIDI    0x03
#define USB_AUDIO_CS_INTERFACE     0x24
#define USB_AUDIO_CS_ENDPOINT      0x25
#define USB_AUDIO_HEADER           0x01
#define USB_MIDI_STREAMING_HEADER  0x01
#define USB_MIDI_IN_JACK           0x02
#define USB_MIDI_OUT_JACK          0x03
#define USB_MIDI_JACK_EMBEDDED     0x01
#define USB_MIDI_JACK_EXTERNAL     0x02
#define USB_MIDI_ENDPOINT_GENERAL  0x01

enum {
    UsbMidiStringLanguage,
    UsbMidiStringManufacturer,
    UsbMidiStringProduct,
};

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdADC;
    uint16_t wTotalLength;
    uint8_t bInCollection;
    uint8_t baInterfaceNr;
} __attribute__((packed)) UsbAudioControlHeader;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdMSC;
    uint16_t wTotalLength;
} __attribute__((packed)) UsbMidiStreamingHeader;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t iJack;
} __attribute__((packed)) UsbMidiInJack;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t bNrInputPins;
    uint8_t baSourceID;
    uint8_t baSourcePin;
    uint8_t iJack;
} __attribute__((packed)) UsbMidiOutJack;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bNumEmbMIDIJack;
    uint8_t baAssocJackID;
} __attribute__((packed)) UsbMidiEndpoint;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
    uint8_t bRefresh;
    uint8_t bSynchAddress;
} __attribute__((packed)) UsbMidiStandardEndpoint;

typedef struct {
    UsbMidiStreamingHeader header;
    UsbMidiInJack input_external;
    UsbMidiOutJack output_embedded;
} __attribute__((packed)) UsbMidiJackDescriptors;

typedef struct {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor audio_control_interface;
    UsbAudioControlHeader audio_control_header;
    struct usb_interface_descriptor midi_streaming_interface;
    UsbMidiJackDescriptors midi_jacks;
    UsbMidiStandardEndpoint bulk_in;
    UsbMidiEndpoint midi_bulk_in;
} __attribute__((packed)) UsbMidiConfigDescriptor;

static const struct usb_device_descriptor usb_midi_device_descriptor = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = USB_MIDI_EP0,
    .idVendor = USB_MIDI_VID,
    .idProduct = USB_MIDI_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = UsbMidiStringManufacturer,
    .iProduct = UsbMidiStringProduct,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

static const UsbMidiConfigDescriptor usb_midi_config_descriptor = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(UsbMidiConfigDescriptor),
            .bNumInterfaces = 2,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .audio_control_interface =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 0,
            .bInterfaceClass = USB_AUDIO_CLASS,
            .bInterfaceSubClass = USB_AUDIO_SUBCLASS_CONTROL,
            .bInterfaceProtocol = USB_PROTO_NONE,
            .iInterface = 0,
        },
    .audio_control_header =
        {
            .bLength = sizeof(UsbAudioControlHeader),
            .bDescriptorType = USB_AUDIO_CS_INTERFACE,
            .bDescriptorSubtype = USB_AUDIO_HEADER,
            .bcdADC = VERSION_BCD(1, 0, 0),
            .wTotalLength = sizeof(UsbAudioControlHeader),
            .bInCollection = 1,
            .baInterfaceNr = 1,
        },
    .midi_streaming_interface =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 1,
            .bAlternateSetting = 0,
            .bNumEndpoints = 1,
            .bInterfaceClass = USB_AUDIO_CLASS,
            .bInterfaceSubClass = USB_AUDIO_SUBCLASS_MIDI,
            .bInterfaceProtocol = USB_PROTO_NONE,
            .iInterface = 0,
        },
    .midi_jacks =
        {
            .header =
                {
                    .bLength = sizeof(UsbMidiStreamingHeader),
                    .bDescriptorType = USB_AUDIO_CS_INTERFACE,
                    .bDescriptorSubtype = USB_MIDI_STREAMING_HEADER,
                    .bcdMSC = VERSION_BCD(1, 0, 0),
                    .wTotalLength = sizeof(UsbMidiJackDescriptors),
                },
            .input_external =
                {
                    .bLength = sizeof(UsbMidiInJack),
                    .bDescriptorType = USB_AUDIO_CS_INTERFACE,
                    .bDescriptorSubtype = USB_MIDI_IN_JACK,
                    .bJackType = USB_MIDI_JACK_EXTERNAL,
                    .bJackID = 1,
                    .iJack = 0,
                },
            .output_embedded =
                {
                    .bLength = sizeof(UsbMidiOutJack),
                    .bDescriptorType = USB_AUDIO_CS_INTERFACE,
                    .bDescriptorSubtype = USB_MIDI_OUT_JACK,
                    .bJackType = USB_MIDI_JACK_EMBEDDED,
                    .bJackID = 2,
                    .bNrInputPins = 1,
                    .baSourceID = 1,
                    .baSourcePin = 1,
                    .iJack = 0,
                },
        },
    .bulk_in =
        {
            .bLength = sizeof(UsbMidiStandardEndpoint),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = USB_MIDI_EP_IN,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = USB_MIDI_EP_SIZE,
            .bInterval = 0,
            .bRefresh = 0,
            .bSynchAddress = 0,
        },
    .midi_bulk_in =
        {
            .bLength = sizeof(UsbMidiEndpoint),
            .bDescriptorType = USB_AUDIO_CS_ENDPOINT,
            .bDescriptorSubtype = USB_MIDI_ENDPOINT_GENERAL,
            .bNumEmbMIDIJack = 1,
            .baAssocJackID = 2,
        },
};

static const struct usb_string_descriptor usb_midi_manufacturer_string =
    USB_STRING_DESC("Flipper Devices Inc.");
static const struct usb_string_descriptor usb_midi_product_string =
    USB_STRING_DESC("Flipper RF MIDI");

typedef struct {
    usbd_device* device;
    FuriSemaphore* tx_ready;
    volatile bool configured;
    volatile bool connected;
} UsbMidiTxState;

static UsbMidiTxState usb_midi;

static void usb_midi_init(usbd_device* device, FuriHalUsbInterface* interface, void* context);
static void usb_midi_deinit(usbd_device* device);
static void usb_midi_wakeup(usbd_device* device);
static void usb_midi_suspend(usbd_device* device);
static usbd_respond usb_midi_configure(usbd_device* device, uint8_t config);
static usbd_respond
    usb_midi_control(usbd_device* device, usbd_ctlreq* request, usbd_rqc_callback* callback);

FuriHalUsbInterface rf_midi_usb_interface = {
    .init = usb_midi_init,
    .deinit = usb_midi_deinit,
    .wakeup = usb_midi_wakeup,
    .suspend = usb_midi_suspend,
    .dev_descr = (struct usb_device_descriptor*)&usb_midi_device_descriptor,
    .cfg_descr = (void*)&usb_midi_config_descriptor,
};

static void usb_midi_tx_complete(usbd_device* device, uint8_t event, uint8_t endpoint) {
    UNUSED(device);
    UNUSED(endpoint);

    if((event == usbd_evt_eptx) && usb_midi.tx_ready) {
        furi_semaphore_release(usb_midi.tx_ready);
    }
}

static void usb_midi_init(usbd_device* device, FuriHalUsbInterface* interface, void* context) {
    UNUSED(context);

    interface->str_manuf_descr = (void*)&usb_midi_manufacturer_string;
    interface->str_prod_descr = (void*)&usb_midi_product_string;
    interface->str_serial_descr = NULL;

    usb_midi.device = device;
    usb_midi.configured = false;
    usb_midi.connected = false;
    if(!usb_midi.tx_ready) {
        usb_midi.tx_ready = furi_semaphore_alloc(1, 1);
    }

    usbd_reg_config(device, usb_midi_configure);
    usbd_reg_control(device, usb_midi_control);
    usbd_connect(device, true);
}

static void usb_midi_deinit(usbd_device* device) {
    usb_midi.connected = false;
    usb_midi.configured = false;
    usb_midi.device = NULL;

    usbd_reg_config(device, NULL);
    usbd_reg_control(device, NULL);
    usbd_reg_endpoint(device, USB_MIDI_EP_IN, NULL);

    if(usb_midi.tx_ready) {
        furi_semaphore_free(usb_midi.tx_ready);
        usb_midi.tx_ready = NULL;
    }
}

static void usb_midi_wakeup(usbd_device* device) {
    UNUSED(device);
    usb_midi.connected = usb_midi.configured;
}

static void usb_midi_suspend(usbd_device* device) {
    UNUSED(device);
    usb_midi.connected = false;
    // An in-flight IN transfer can be lost on suspend/disconnect. Match the HAL
    // HID driver: release the token so resume cannot leave MIDI permanently busy.
    if(usb_midi.tx_ready) furi_semaphore_release(usb_midi.tx_ready);
}

static usbd_respond usb_midi_configure(usbd_device* device, uint8_t config) {
    if(config == 0) {
        usb_midi.connected = false;
        usb_midi.configured = false;
        usbd_reg_endpoint(device, USB_MIDI_EP_IN, NULL);
        usbd_ep_deconfig(device, USB_MIDI_EP_IN);
        if(usb_midi.tx_ready) {
            furi_semaphore_release(usb_midi.tx_ready);
        }
        return usbd_ack;
    }

    if(config == 1) {
        if(!usbd_ep_config(device, USB_MIDI_EP_IN, USB_EPTYPE_BULK, USB_MIDI_EP_SIZE)) {
            return usbd_fail;
        }
        usbd_reg_endpoint(device, USB_MIDI_EP_IN, usb_midi_tx_complete);
        usb_midi.configured = true;
        usb_midi.connected = true;
        if(usb_midi.tx_ready) {
            furi_semaphore_release(usb_midi.tx_ready);
        }
        return usbd_ack;
    }

    return usbd_fail;
}

static usbd_respond
    usb_midi_control(usbd_device* device, usbd_ctlreq* request, usbd_rqc_callback* callback) {
    UNUSED(device);
    UNUSED(request);
    UNUSED(callback);
    return usbd_fail;
}

bool usb_midi_tx_control_change(uint8_t controller, uint8_t value, uint32_t timeout_ms) {
    if(!usb_midi_tx_is_connected() || !usb_midi.tx_ready) {
        return false;
    }

    if(furi_semaphore_acquire(usb_midi.tx_ready, furi_ms_to_ticks(timeout_ms)) != FuriStatusOk) {
        return false;
    }

    if(!usb_midi_tx_is_connected()) {
        furi_semaphore_release(usb_midi.tx_ready);
        return false;
    }

    const uint8_t packet[4] = {
        0x0B, // cable 0, Control Change
        0xB0, // MIDI channel 1
        controller,
        value,
    };

    const int32_t written = usbd_ep_write(usb_midi.device, USB_MIDI_EP_IN, packet, sizeof(packet));
    if(written != (int32_t)sizeof(packet)) {
        furi_semaphore_release(usb_midi.tx_ready);
        return false;
    }

    return true;
}

bool usb_midi_tx_is_connected(void) {
    // USB reset clears the core configuration before SET_CONFIGURATION arrives.
    // The HAL owns the reset callback, so consult the core state as well.
    return usb_midi.connected && usb_midi.device && usb_midi.device->status.device_cfg == 1;
}
