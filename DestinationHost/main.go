package main

import (
	"fmt"
	"log"
	"strings"

	"github.com/charmbracelet/bubbles/table"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"go.bug.st/serial"
)

var SERIAL_START = []byte{0xAA, 0xBB, 0xCC}
var SERIAL_END = []byte{0xDD, 0xEE, 0xFF}
var SUBSTART = []byte{0x11, 0x22}
var SUBEND = []byte{0x33, 0x44}

type serialMsg []byte

type model struct {
	table        table.Model
	port         serial.Port
	ch           chan serialMsg
	buffer       []byte
	messages     [][]byte
	detailStyle  lipgloss.Style
	infoStyle    lipgloss.Style
	connected    bool
	messageCount int
	width        int
	height       int
}

func main() {
	portName := "/dev/ttyUSB2"
	baudRate := 115200

	mode := &serial.Mode{BaudRate: baudRate}
	port, err := serial.Open(portName, mode)
	connected := err == nil
	if err != nil {
		log.Println("Warning: could not open serial port:", err)
	}
	if port != nil {
		defer port.Close()
	}

	columns := []table.Column{
		{Title: "Hex", Width: 50},
		{Title: "ASCII", Width: 30},
	}

	t := table.New(
		table.WithColumns(columns),
		table.WithRows([]table.Row{}),
		table.WithFocused(true),
	)

	styles := table.DefaultStyles()
	styles.Header = styles.Header.
		BorderStyle(lipgloss.NormalBorder()).
		BorderForeground(lipgloss.Color("240")).
		BorderBottom(true)
	styles.Selected = styles.Selected.
		Foreground(lipgloss.Color("229")).
		Background(lipgloss.Color("57"))

	t.SetStyles(styles)

	m := model{
		table:        t,
		port:         port,
		ch:           make(chan serialMsg, 100),
		detailStyle:  lipgloss.NewStyle().BorderStyle(lipgloss.NormalBorder()).Padding(1),
		infoStyle:    lipgloss.NewStyle().Foreground(lipgloss.Color("240")).Background(lipgloss.Color("236")).Padding(0, 1),
		connected:    connected,
		messageCount: 0,
	}

	p := tea.NewProgram(m, tea.WithAltScreen())
	if err := p.Start(); err != nil {
		log.Fatal(err)
	}
}

// ------------------- Serial Handling -------------------

func (m model) Init() tea.Cmd {
	if m.connected {
		go func(port serial.Port, ch chan<- serialMsg) {
			buf := make([]byte, 256)
			for {
				n, err := port.Read(buf)
				if err != nil {
					continue
				}
				if n > 0 {
					data := make([]byte, n)
					copy(data, buf[:n])
					ch <- serialMsg(data)
				}
			}
		}(m.port, m.ch)
	}

	return readSerialCmd(m.ch)
}

func readSerialCmd(ch <-chan serialMsg) tea.Cmd {
	return func() tea.Msg {
		return <-ch
	}
}

// ------------------- Message Parsing -------------------

func indexOf(data []byte, pattern []byte) int {
	for i := 0; i <= len(data)-len(pattern); i++ {
		match := true
		for j := 0; j < len(pattern); j++ {
			if data[i+j] != pattern[j] {
				match = false
				break
			}
		}
		if match {
			return i
		}
	}
	return -1
}

func extractMessages(buf []byte) (messages [][]byte, remaining []byte) {
	start := 0
	for {
		idxStart := indexOf(buf[start:], SERIAL_START)
		if idxStart == -1 {
			break
		}
		idxStart += start

		idxEnd := indexOf(buf[idxStart:], SERIAL_END)
		if idxEnd == -1 {
			break
		}
		idxEnd += idxStart + len(SERIAL_END)

		messages = append(messages, buf[idxStart:idxEnd])
		start = idxEnd
	}

	if start < len(buf) {
		remaining = buf[start:]
	} else {
		remaining = nil
	}
	return
}

func removeAll(msg []byte, sub []byte) []byte {
	for {
		idx := indexOf(msg, sub)
		if idx == -1 {
			break
		}
		msg = append(msg[:idx], msg[idx+len(sub):]...)
	}
	return msg
}

func stripMarkers(msg []byte) []byte {
	msg = removeAll(msg, SERIAL_START)
	msg = removeAll(msg, SERIAL_END)
	msg = removeAll(msg, SUBSTART)
	msg = removeAll(msg, SUBEND)
	return msg
}

func parseMessage(msg []byte) table.Row {
	msgClean := stripMarkers(msg)
	return table.Row{joinHex(msgClean), string(msgClean)}
}

func joinHex(b []byte) string {
	hexs := make([]string, len(b))
	for i, c := range b {
		hexs[i] = fmt.Sprintf("%02X", c)
	}
	return strings.Join(hexs, " ")
}

// ------------------- Bubble Tea Update -------------------

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	var cmd tea.Cmd

	switch msg := msg.(type) {
	case serialMsg:
		m.buffer = append(m.buffer, msg...)
		newMsgs, remaining := extractMessages(m.buffer)
		m.buffer = remaining

		for _, message := range newMsgs {
			row := parseMessage(message)
			rows := append(m.table.Rows(), row)
			m.table.SetRows(rows)
			m.messages = append(m.messages, message)
			m.messageCount++
		}

		return m, readSerialCmd(m.ch)

	case tea.KeyMsg:
		switch msg.String() {
		case "q", "ctrl+c":
			return m, tea.Quit
		case "up", "down", "pgup", "pgdown":
			m.table, cmd = m.table.Update(msg)
		}
		return m, cmd

	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height

		// Resize table to fill half the screen width and most of height
		m.table.SetWidth(msg.Width / 2)
		m.table.SetHeight(msg.Height - 3)

		return m, nil
	}

	m.table, cmd = m.table.Update(msg)
	return m, cmd
}

// ------------------- Bubble Tea View -------------------

func (m model) View() string {
	// Detail panel for selected message
	detail := ""
	if len(m.table.Rows()) > 0 && m.table.Cursor() < len(m.messages) {
		msg := m.messages[m.table.Cursor()]
		cleanMsg := stripMarkers(msg)
		detail = m.detailStyle.Width(m.width/2 - 2).Render(fmt.Sprintf("Full Message:\n%s", string(cleanMsg)))
	}

	// Table + detail side by side
	content := lipgloss.JoinHorizontal(
		lipgloss.Top,
		m.table.View(),
		detail,
	)

	// Single bottom info bar
	info := fmt.Sprintf(
		"Serial: %s | Sending message %d off to MeshWay servers...",
		func() string {
			if m.connected {
				return "Connected"
			}
			return "Disconnected"
		}(),
		m.messageCount,
	)
	infoBar := m.infoStyle.Width(m.width).Render(info)

	// Stack table+detail above info bar
	return lipgloss.JoinVertical(
		lipgloss.Left,
		content,
		infoBar,
	)
}
