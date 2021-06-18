# Valorant Input Log Proof of Concept
This was a C++ console application designed to log inputs for Valorant with high precision.
Unlike generic keyloggers, this application is only active when it detects that Valorant is the focused window.
It tracks relative mouse movement (instead of absolute cursor position) and individual key events and logs them with the time in ms that the event happened.

 ### Findings
 - I don't like C++
 - Valorant does not reset the cursor position to the center of the window so using
   absolute cursor position may be inaccurate if the user is moving continuously in one direction
 - Raw mouse input events happen really, really often so summing them and logging the sum every 50ms or so seems to be a good approach
 - The file format requires reading from the very start to maintain state and variable packet lengths
    - COBS could be utilized to ensure the file could be read from the middle and quickly find a packet
    - Keyframe packets may be useful to ensure the file would only need to be read backwards a maximum amount (instead of from the start)
 - The file format is a suitable candidate for streaming compression. Something I really don't want to figure out how to do in C++