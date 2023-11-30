var ctr = 0
import string
import json
import path

class EBC_FileLogger : Driver
    var elapsed_time
    static var sampling_time = 5
    var filepnt 

    def init()
        self.elapsed_time = 0

        try
            if (path.exists("./logger.csv"))
                self.filepnt = open("./logger.csv","a+")
            else
                self.filepnt = open("./logger.csv","w")
                self.filepnt.write("data;dispositivo;temperatura;umidita")
                self.filepnt.flush()
            end
        except .. as e,m
            print("Error initing file")
        end
        
    end

    def write_to_file(line)
        self.filepnt.write(line)
        self.filepnt.flush()
    end
    
    def every_second()
        var temp 
        var hum  
        var time 
        var press
        self.elapsed_time += 1
        if self.elapsed_time % self.sampling_time == 0 #take and write a reading every x sec
            var sensors = json.load(tasmota.read_sensors())
            print(sensors)
            #loop though all supported sensors
            if (sensors.contains('EBC'))
                temp = sensors['EBC']['Temperature']
                hum = sensors['EBC']['Humidity']
                time = sensors['Time']
                time = string.replace(time, "T", " ")
                self.write_to_file(str(time) +";EBC;"+str(temp) + ";" + str(hum)+"\r\n" )
            elif (sensors.contains('BMP280'))
                    temp = sensors['BMP280']['Temperature']
                    hum = sensors['BMP280']['Humidity']
                    time = sensors['Time']
                    time = string.replace(time, "T", " ")
                    self.write_to_file(str(time) +";BMP280;"+str(temp) + ";" + str(hum)+"\r\n" )
            else
                print("No sensors found!")
            end

        end
    end

end

EBC_FileLogger = EBC_FileLogger()
tasmota.add_driver(EBC_FileLogger)