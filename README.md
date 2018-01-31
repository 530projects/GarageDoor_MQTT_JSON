# GarageDoor_MQTT_JSON
<br>
This is the software that I used for my garage door sensor. I have combined it into one file for easy copy/paste action. 
<br>
I know that the code is really ugly (especially the ping sensor averaging), but it seems to work fairly well. 
<br>
See it in action on my youtube channel here: https://www.youtube.com/watch?v=2r5pLsRWH9g
<br>
<b>To_Do:</b>I need to add debouncing logic to the door sensor. As occasionally I will get a random false open state immediately followed by close state from my reed switch. I have taken care of it in my home automation system, but it really just need to be debounced in this code. 
