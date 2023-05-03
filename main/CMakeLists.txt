cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS
                "sd_record.c" 
                "espnow_recv.c" 
                
                "espnow_send.c" 
                "espnow_mic.c" 
                "config.c" 
                "sd_record.c"
                       INCLUDE_DIRS ".")

