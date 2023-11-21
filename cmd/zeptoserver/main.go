package main

import (
	"flag"
	"fmt"
	"io"
	"mime"
	"mime/multipart"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/gorilla/websocket"
	log "github.com/schollz/logger"
)

var flagLogLevel string
var connections map[string]*websocket.Conn

func init() {
	connections = make(map[string]*websocket.Conn)
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)
	Serve()
}

func Serve() {
	port := 8098
	log.Infof("listening on :%d", port)
	http.HandleFunc("/", handler)
	http.ListenAndServe(fmt.Sprintf(":%d", port), nil)
}

func handler(w http.ResponseWriter, r *http.Request) {
	t := time.Now().UTC()
	err := handle(w, r)
	if err != nil {
		w.Write([]byte(err.Error()))
		log.Error(err)
	}
	log.Infof("%v %v %v %s\n", r.RemoteAddr, r.Method, r.URL.Path, time.Since(t))
}

func handle(w http.ResponseWriter, r *http.Request) (err error) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
	w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization")

	// very special paths
	if r.Method == "POST" {
		// POST file
		// this is called from browser upload
		return handleUpload(w, r)
	} else if r.URL.Path == "/ws" {
		return handleWebsocket(w, r)
	} else {
		if r.URL.Path == "/" {
			r.URL.Path = "/index.html"
		}
		mime := mime.TypeByExtension(filepath.Ext(r.URL.Path))
		w.Header().Set("Content-Type", mime)
		var b []byte
		b, err = os.ReadFile(r.URL.Path[1:])
		if err != nil {
			return
		}
		w.Write(b)
	}

	return
}

var upgrader = websocket.Upgrader{} // use default options

type Message struct {
	Action  string   `json:"action"`
	Message string   `json:"message"`
	Error   string   `json:"error"`
	Success bool     `json:"success"`
	File    FileData `json:"file"`
}

type FileData struct {
	OriginalFilename string `json:"originalFilename"`
	Filename         string `json:"filename"`
	Size             int64  `json:"size"`
}

func handleWebsocket(w http.ResponseWriter, r *http.Request) (err error) {
	query := r.URL.Query()
	if _, ok := query["id"]; !ok {
		err = fmt.Errorf("no id")
		return
	}

	// use gorilla to open websocket
	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		return
	}
	defer func() {
		c.Close()
		delete(connections, query["id"][0])
	}()
	connections[query["id"][0]] = c

	for {
		var message Message
		err := c.ReadJSON(&message)
		if err != nil {
			break
		}
		log.Debugf("message: %+v", message)
		if message.Action == "waveform" {
			c.WriteJSON(Message{
				Action: "waveform",
				File: FileData{
					OriginalFilename: "amen.wav",
					Filename:         "amen.wav",
					Size:             0,
				},
				Success: true,
			})
		}
	}
	return
}

func handleUpload(w http.ResponseWriter, r *http.Request) (err error) {
	// get the url query parameters from the request r
	query := r.URL.Query()
	if _, ok := query["id"]; !ok {
		err = fmt.Errorf("no id")
		return
	}
	fmt.Println(query["id"][0])

	// Parse the multipart form data
	err = r.ParseMultipartForm(10 << 20) // 10 MB limit
	if err != nil {
		http.Error(w, "Unable to parse form", http.StatusBadRequest)
		return
	}

	// Retrieve the files from the form data
	files := r.MultipartForm.File["files"]

	// Process each file
	for _, file := range files {
		// Open the uploaded file
		var uploadedFile multipart.File
		uploadedFile, err = file.Open()
		if err != nil {
			return
		}
		defer uploadedFile.Close()

		// Read the file content
		var fileContent []byte
		fileContent, err = io.ReadAll(uploadedFile)
		if err != nil {
			return
		}

		// Process the file content as needed
		fmt.Printf("File name: %s, Size: %d bytes\n", file.Filename, len(fileContent))
		// save file locally
		err = os.WriteFile(file.Filename, fileContent, 0666)
		if err != nil {
			return
		}
	}

	// Send a response
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"success":true}`))
	return
}
