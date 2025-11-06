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

func publish(input Input, client mqtt.Client) {
	// 0 denotes minimum Quality of Service (QoS), fastest option
	token := client.Publish("topic/control", 0, false, input)
	token.Wait()
	time.Sleep(time.Second)
}

func processKeystroke(key keys.Key, client mqtt.Client) (stop bool, err error) {
	switch key.Code {

	// Exit on q
	case keys.RuneKey:
		if key.String() == "q" {
			return true, nil
		}

	// Process keys
	case keys.Up:
		// inputChannel <- UP
	case keys.Down:
		// inputChannel <- DOWN
	case keys.Left:
		// inputChannel <- LEFT
	case keys.Right:
		// inputChannel <- RIGHT
	}

	return false, nil
}

const BROKER_DNS = "localhost" // localhost for testing... switch to EC2 internal DNS name later
const PORT = 1883

func main() {
	opts := mqtt.NewClientOptions()

	opts.AddBroker(fmt.Sprintf("tcp://%s:%d", BROKER_DNS, PORT))
	opts.SetClientID("boat_controller")
	opts.OnConnect = func(client mqtt.Client) {
		fmt.Println("Connected...")
	}

	client := mqtt.NewClient(opts)

	if token := client.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	fmt.Println("Taking input...")
	keyboard.Listen(func(key keys.Key) (stop bool, err error) {
		return processKeystroke(key, client)
	})
}
