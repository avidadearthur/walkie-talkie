cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS
                "sd_record.c" 
                "udp_server.c" 
                "main.c" 
                "espnow_mic.c" 
                "config.c" 
                "udp_client.c"
                       INCLUDE_DIRS ".")

