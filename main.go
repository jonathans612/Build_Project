package main

import (
	"fmt"
	"time"

	"atomicgo.dev/keyboard"
	"atomicgo.dev/keyboard/keys"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

type Input = string

const (
	UP    Input = "up"
	DOWN  Input = "down"
	LEFT  Input = "left"
	RIGHT Input = "right"
)

var client mqtt.Client

func publish(input Input) {
	// 0 denotes minimum Quality of Service (QoS), fastest option
	token := client.Publish("topic/control", 0, false, input)
	token.Wait()
	time.Sleep(time.Second)
}

func processKeystroke(key keys.Key) (stop bool, err error) {
	switch key.Code {

	// Exit on q
	case keys.RuneKey:
		if key.String() == "q" {
			return true, nil
		}

	// Process keys
	case keys.Up:
		fmt.Printf("[INPUT] %s\n", UP)
		publish(UP)
	case keys.Down:
		fmt.Printf("[INPUT] %s\n", DOWN)
		publish(DOWN)
	case keys.Left:
		fmt.Printf("[INPUT] %s\n", LEFT)
		publish(LEFT)
	case keys.Right:
		fmt.Printf("[INPUT] %s\n", RIGHT)
		publish(RIGHT)
	}

	return false, nil
}

// TODO -- Diagnose connection issues on localhost. perhaps try mosquitto_pub command?

const BROKER_DNS = "localhost" // localhost for testing... switch to EC2 internal DNS name later
const PORT = 1883

func main() {
	opts := mqtt.NewClientOptions()

	opts.AddBroker(fmt.Sprintf("tcp://%s:%d", BROKER_DNS, PORT))
	opts.SetClientID("boat_controller")
	opts.SetUsername("boat")
	opts.SetPassword("initbuild2025")
	opts.OnConnect = func(client mqtt.Client) {
		fmt.Println("Connected...")
	}

	client = mqtt.NewClient(opts)

	if token := client.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	fmt.Println("Taking input...")
	keyboard.Listen(processKeystroke)
}
