#include "aecp-aem-descriptors.h"
#include "avb.h"
#include "internal.h"
//TODO: implement entity parrser - define struct

struct avb_entity_config {
    char entity_name[64];
    char vendor_name[64];
    char group_name[64];
    char serial_number[64];
    char firmware_version[64];
    uint32_t entity_capabilities;
    uint32_t talker_capabilities;
    uint32_t listener_capabilities;
    uint32_t controller_capabilities;
    // Add other static Entity fields if strictly necessary
};

//TODO: make function that returns that struct - neeed to take in the
struct avb_aem_desc_entity conf_load_entity (struct pw_properties *props) {
	const char *str;
	if ((str = pw_properties_get(props, "avb.properties.entity")) != NULL) {
		pw_log_warn("No entity model in avb.properties is set - using all default values");
	}
	struct pw_properties *entity_props = pw_properties_update_string(entity_props, str, strlen(str));

	struct avb_aem_desc_entity entity_conf =
	{
                .entity_id = htobe64(DSC_ENTITY_MODEL_ENTITY_ID),
                .entity_model_id = htobe64(DSC_ENTITY_MODEL_ID),
                .entity_capabilities = htonl(DSC_ENTITY_MODEL_ENTITY_CAPABILITIES),

                .talker_stream_sources = htons(DSC_ENTITY_MODEL_TALKER_STREAM_SOURCES),
                .talker_capabilities = htons(DSC_ENTITY_MODEL_TALKER_CAPABILITIES),

                .listener_stream_sinks = htons(DSC_ENTITY_MODEL_LISTENER_STREAM_SINKS),
                .listener_capabilities = htons(DSC_ENTITY_MODEL_LISTENER_CAPABILITIES),

                .controller_capabilities = htons(DSC_ENTITY_MODEL_CONTROLLER_CAPABILITIES),

                .available_index = htonl(DSC_ENTITY_MODEL_AVAILABLE_INDEX),
                .association_id = htobe64(DSC_ENTITY_MODEL_ASSOCIATION_ID),

                //.entity_name = DSC_ENTITY_MODEL_ENTITY_NAME,
                .entity_name = pw_properties_get(entity_props, "entity_name"),
                .vendor_name_string = htons(DSC_ENTITY_MODEL_VENDOR_NAME_STRING),
                .model_name_string = htons(DSC_ENTITY_MODEL_MODEL_NAME_STRING),
                .firmware_version = DSC_ENTITY_MODEL_FIRMWARE_VERSION,
                .group_name = DSC_ENTITY_MODEL_GROUP_NAME,
                .serial_number = DSC_ENTITY_MODEL_SERIAL_NUMBER,
                .configurations_count = htons(DSC_ENTITY_MODEL_CONFIGURATIONS_COUNT),
                .current_configuration = htons(DSC_ENTITY_MODEL_CURRENT_CONFIGURATION)
        });

	// TODO: turn spa item into dictt
	//TODO: get props

	//TODO: test 1: load in new entity name
/*
	uint64_t entity_id;
        uint64_t entity_model_id;
        uint32_t entity_capabilities;
        uint16_t talker_stream_sources;
        uint16_t talker_capabilities;
        uint16_t listener_stream_sinks;
        uint16_t listener_capabilities;
        uint32_t controller_capabilities;
        uint32_t available_index;
        uint64_t association_id;
        char entity_name[64];
        uint16_t vendor_name_string;
        uint16_t model_name_string;
        char firmware_version[64];
        char group_name[64];
        char serial_number[64];
        uint16_t configurations_count;
        uint16_t current_configuration;
	*/

	//TODO: fill in NULL to any non filled param


//TODO: return struct
	return entity_conf;
}
