import machine
import utime
import ssd1306
import framebuf  # Added this import

# I2C Setup (SDA=GP0, SCL=GP1)
i2c = machine.I2C(0, sda=machine.Pin(0), scl=machine.Pin(1), freq=400000)
oled = ssd1306.SSD1306_I2C(128, 64, i2c)
rtc = machine.RTC()

def draw_scaled_text(text, x_pos, y_pos, scale):
    """Draws text larger by scaling the built-in font."""
    for char in text:
        # Create a tiny 8x8 buffer for a single character
        char_buf = bytearray(8) 
        # Fixed the module reference here:
        fb = framebuf.FrameBuffer(char_buf, 8, 8, framebuf.MONO_VLSB)
        fb.text(char, 0, 0, 1)
        
        for y in range(8):
            for x in range(8):
                if fb.pixel(x, y):
                    # Draw a scaled block
                    for sx in range(scale):
                        for sy in range(scale):
                            # Ensure we don't draw outside the 128x64 boundary
                            draw_x = x_pos + (x * scale) + sx
                            draw_y = y_pos + (y * scale) + sy
                            if draw_x < 128 and draw_y < 64:
                                oled.pixel(draw_x, draw_y, 1)
        x_pos += 8 * scale 

while True:
    t = rtc.datetime()
    # Format HH:MM:SS
    time_str = "{:02d}:{:02d}:{:02d}".format(t[4], t[5], t[6])
    # Format YYYY-MM-DD
    date_str = "{:04d}-{:02d}-{:02d}".format(t[0], t[1], t[2])

    oled.fill(0)
    
    # Scale 2 makes the text twice as big. 
    # (8 pixels * 8 chars * 2 = 128 pixels wide, fits perfectly!)
    draw_scaled_text(time_str, 0, 15, scale=2) 
    
    # Date at the very bottom
    oled.text(date_str, 24, 52)
    
    oled.show()
    utime.sleep(1)
