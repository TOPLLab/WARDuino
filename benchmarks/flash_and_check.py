import serial
import time



def awaitOutput(serial:serial.Serial, target:str):
    target = target.encode('utf-8')
    curpos = 0
    while True:
        x = serial.read()[0]
        if(x == target[curpos]):
            curpos+=1
            if curpos == len(target):
                curpos = 0
                return
        else:
            curpos = 0


if __name__ == "__main__":
    
    with serial.Serial('/dev/ttyUSB0', 115200) as serial:
        awaitOutput(serial,"START")
        startTime = time.monotonic()
        awaitOutput(serial,"DONE")
        endTime = time.monotonic()
        print(endTime-startTime)
            
