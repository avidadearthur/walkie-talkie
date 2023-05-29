cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS
                "udp_server.c" 
                "udp_client.c"
                "main.c" 
                "espnow_mic.c" 
                "config.c" 
                       INCLUDE_DIRS ".")

