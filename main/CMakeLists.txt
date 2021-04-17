idf_component_register(SRCS "main.cpp" "SSD1306.cpp" "OLED.cpp"
                    INCLUDE_DIRS "." "include"
                    EMBED_TXTFILES web/index.html
                    )
