cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS
                "espnow_recv.c" 
                "main.c"   
                "espnow_send.c" 
                "espnow_mic.c" 
                "config.c" 
                       INCLUDE_DIRS ".")

