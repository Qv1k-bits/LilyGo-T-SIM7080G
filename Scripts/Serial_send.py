import serial.tools.list_ports
import time

ports = serial.tools.list_ports.comports()
serialInst = serial.Serial()

portsList = []

for onePort in ports:
    portsList.append(str(onePort))
    print(str(onePort))

val = input("Select Port: COM")

for x in range(0, len(portsList)):
    if portsList[x].startswith("COM" + str(val)):
        portVar = "COM" + str(val)
        print(portVar)

serialInst.baudrate = 115200
serialInst.port = portVar
serialInst.open()

while True:
    try:
        command = input("Input AT command: ")
        print(command)
        if command == "1":
            ATcommand = "AT+QMTCFG=?\r"
        else:
            ATcommand = command+"\r"

        serialInst.write(command.encode('utf-8'))
        time.sleep(7)
        res = serialInst.read_all()
        #print(res)
        print(res.decode("utf-8").strip())

        if command == 'exit':
            exit()
    except KeyboardInterrupt:
        break # exit the loop when Ctrl+C is pressed

serialInst.close() # close the serial port