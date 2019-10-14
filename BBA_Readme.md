Detailed Information about BBA Emulation    

Nintendont supports BBA Emulation in the following titles:  
Mario Kart: Double Dash!!  
Kirby Air Ride  
1080 Avalanche  
PSO Episode 1&2  
PSO Episode III  
Homeland    

It works just like a real BBA and works from both a USB LAN adapter and over WLAN.  
To enable BBA Emulation, simply go into nintendonts settings and enable it.  
If you are on a Wii, you can also switch the Network Profile as configued in your Wii Settings used for it.  
This can be useful if the profile cannot be selected as a default in the Wii Settings when it has no internet access and you just need LAN acccess.  
On WiiU this option is not available, but it is not needed because you can default a profile regardless of internet access or not.      


Game specific notes:  
For Mario Kart, Kirby Air Ride and 1080 Avalance, when you want to connect to a real GameCube or Dolphin, first go into the LAN Mode on them.  
After THEY start looking for other consoles, only THEN connect the other nintendont instances.  
If you do not follow that order, nintendont will not be able to see the other consoles, possibly due to a bug in the wii firmware.  
Important for PAL users: Both Mario Kart and 1080 Avalanche have to be set to the same 50/60Hz setting on all consoles on game boot in order to be found by other consoles, if one of your consoles is set to a different refresh rate, it wont be detected by the others when they search.  
Interestingly, this does not apply for Kirby Air Ride which will find each other and work regardless of your 50/60Hz selection.  
You can only connect to other consoles in your local network unless you use a LAN tunneling program such as Xlink Kai just like on a real BBA.  
Also, while WLAN will work, it of course has the potential to run slower.  
Note that 1080 Avalanche has a especially big problem with disconnecting very often, see helper cheats below to help its connection.    

For the Phantasy Star Online games you are able to connect to private servers such as schthack after creating an account on them.  
After making an account, go into the games network settings, set DHCP to auto and do not disconnect, and then enter the DNS Server of the private server you want to join.  
You can also connect a USB Keyboard and use it ingame when you have native controls turned off without any further setup required.  
With native controls turned on it will instead accept a real GameCube Keyboard in the controller port of original Wiis if you happen to own one.  
The keyboard layout will be for a japanese keyboard by default, see helper cheats below if you want to use a different layout.    

For Homeland, you can use the game as both server and client when you enable the direct IP mode in game. Just set its network settings to DHCP auto just like PSO.  
Then you should be able to play with it just like on a real GameCube or Dolphin.      


Helper Cheats:  
Below you can find various cheats for 1080 Avalanche and the Phantasy Star Online games.  
You can convert those to a .gct file using this website: https://geckocodes.org/index.php?gct=  
Simply add the codes, edit the PSO ones as required, download the .gct and put it named as "game.gct" next to your "game.iso" on whatever device you have your games on.  
Of course, make sure to enable cheats in nintendonts settings so they have an effect.    

Network codes for 1080 Avalanche:    

The following codes attempt to wait for the opponents input instead of immediately disconnecting when it is not present.  
This can potentially help your connection to disconnect less, note though the game will probably still disconnect a lot.  

1080 Wait for Connection (NTSC-U) [FIX94]  
04104604 4800009C  
04104678 48000028    

1080 Wait for Connection (PAL) [FIX94]  
041051B0 4800009C  
04105224 48000028    

1080 Wait for Connection (NTSC-J) [FIX94]  
041092F4 4800009C  
04109368 48000028    

Keyboard Layout cheats for the PSO games:    

The following codes change the keyboard layout the game will use.  
Find the code for your version and change X in the second line to your layout:  
0 - for original JP layout  
1 - for US qwerty layout  
2 - for french azerty layout  
3 - for german qwertz layout  
4 - for spanish qwerty layout    

NTSC-U Codes:    

PSO Keyboard Layout Modifier (Ep 1&2 v1.00 NTSC-U) [FIX94]  
203EDC7C 88630001  
043EDC7C 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep 1&2 v1.01 NTSC-U) [FIX94]  
203EDCD4 88630001  
043EDCD4 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep 1&2 Plus NTSC-U) [FIX94]  
203F1558 88630001  
043F1558 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep III NTSC-U) [FIX94]  
2039FAD4 88630001  
0439FAD4 3860000X  
E2000001 80008000    

PAL Codes:    

PSO Keyboard Layout Modifier (Ep 1&2 PAL) [FIX94]  
203EFEC4 88630001  
043EFEC4 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep III PAL) [FIX94]  
203A0978 88630001  
043A0978 3860000X  
E2000001 80008000    

NTSC-J Codes:    

PSO Keyboard Layout Modifier (Ep 1&2 v1.02 NTSC-J) [FIX94]  
203EC9D8 88630001  
043EC9D8 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep 1&2 v1.03 NTSC-J) [FIX94]  
203EF3B4 88630001  
043EF3B4 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep 1&2 Plus v1.04 NTSC-J) [FIX94]  
203F13D8 88630001  
043F13D8 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep 1&2 Plus v1.05 NTSC-J) [FIX94]  
203F1188 88630001  
043F1188 3860000X  
E2000001 80008000    

PSO Keyboard Layout Modifier (Ep III NTSC-J) [FIX94]  
2039EA84 88630001  
0439EA84 3860000X  
E2000001 80008000    

