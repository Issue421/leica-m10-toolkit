/* MIT. Actual accepted C shot snapshot/metadata math, no DNG file/device I/O. */
#include "wb_metadata.h"
int reference_wb(const uint32_t gains[6],const uint8_t original[24],uint8_t out[24]){
 wb_runtime runtime;wb_config config;wb_shot shot;unsigned i;
 if(wb_runtime_init(&runtime)!=WB_OK||wb_config_defaults(&config)!=WB_OK||wb_shot_init(&shot)!=WB_OK)return 1;
 config.enabled=1;for(i=0;i<3;i++)config.gains[i]=(wb_gain){gains[2*i],gains[2*i+1]};
 if(wb_runtime_set(&runtime,&config)!=WB_OK||wb_shot_begin(&shot,&runtime,3)!=WB_OK)return 2;
 /* A later selection is for a later shot; it must not change this snapshot. */
 if(wb_runtime_reset(&runtime)!=WB_OK)return 3;
 return (int)wb_shot_finish(&shot,out,24,original,24,WB_LITTLE_ENDIAN);
}
