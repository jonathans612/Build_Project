package main

import (
	"fmt"

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
}

func processKeystroke(key keys.Key, inputBuffer chan string) (stop bool, err error) {
	switch key.Code {

	// Exit on q
	case keys.RuneKey:
		if key.String() == "q" {
			inputBuffer <- "q"
			return true, nil
		}

	// Process keys
	case keys.Up:
		inputBuffer <- UP
	case keys.Down:
		inputBuffer <- DOWN
	case keys.Left:
		inputBuffer <- LEFT
	case keys.Right:
		inputBuffer <- RIGHT
	}

	return false, nil
}

func processInput(inputBuffer chan string, client mqtt.Client) {
	var input string
	for {
		input = <-inputBuffer

		if input == "q" {
			return
		}

		publish(input, client)
	}

}

const BROKER_DNS = "localhost" // localhost for testing... switch to EC2 elsaticIP DNS name later
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

	inputBuffer := make(chan string, 30)

	go processInput(inputBuffer, client)

	fmt.Println("Taking input...")
	keyboard.Listen(func(key keys.Key) (stop bool, err error) {
		return processKeystroke(key, inputBuffer)
	})

	fmt.Println("Exiting keystroke listener")
}

/*
 *		To listen, open shell and run
 *
 *			mosquitto_sub -h (DNS) -p 1883 -t "topic/control"
 *
 */
