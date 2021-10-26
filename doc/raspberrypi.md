Running on Raspberry PI
===================

For this example used adapter-board: https://www.waveshare.com/rs485-can-hat.htm you can find it on aliexpress too.
After flashing Raspberry image to SD card modify 'config.txt', uncomment this line:

	dtparam=spi=on

Add this line:

	dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25,spimaxfrequency=5000000

Save file and create empty 'SSH' file if you want to remotely login over ethenet to pi.
For Windows (10+) use:

	ssh raspberrypi.local -l pi

Password: raspberry

You can check if CAN driver initialized successfully with:

	dmesg | grep -i '\(can\|spi\)'

Prepare something needed for build

	sudo apt-get install libconfig-dev

Now lets clone socketcand to sd card and build it

	cd boot
	sudo git clone https://github.com/linux-can/socketcand.git
	cd socketcand
	sudo ./autogen.sh
	sudo ./configure
	sudo make

Now you can install it into system

	sudo make install
	
After that you may want to make it run on boot as service, run this command to edit it
	
	sudo systemctl edit --force --full socketcand.service

Replace content with this rescription:

	[Unit]
	Description=CAN ethernet
	After=server.service multi-user.target

	[Service]
	ExecStart=
	ExecStart=-/usr/local/bin/socketcand -i can0
	Restart=always
	TimeoutSec=10

	[Install]
	WantedBy=multi-user.target

Try to start it with

	sudo systemctl daemon-reload
	sudo systemctl start socketcand.service

And listen on UDP default port 42000 for discovery message.

	<CANBeacon name="raspberrypi" type="SocketCAN" description="socketcand">
	<URL>can://192.168.1.221:29536</URL><Bus name="can0"/></CANBeacon>

Note that it should say 'can0' and not 'vcan0'. 
If everything ok so far, what is quite suprisingly, then run this command to activate service on boot.

	sudo systemctl enable socketcand.service
	sudo reboot
	
Check for discovery again.
Now you can connect to IP: raspberrypi.local on TCP port 29536 (default) to start working with remote CAN. Check protocol file for commands and its description.