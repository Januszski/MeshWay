package main

import (
	"fmt"
	"log"
	"strconv"
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
		table:       t,
		port:        port,
		ch:          make(chan serialMsg, 100),
		detailStyle: lipgloss.NewStyle().BorderStyle(lipgloss.NormalBorder()).Padding(1),
		infoStyle:   lipgloss.NewStyle().Foreground(lipgloss.Color("240")).Background(lipgloss.Color("236")).Padding(0, 1),
		connected:   connected,
	}

	p := tea.NewProgram(m, tea.WithAltScreen())
	if err := p.Start(); err != nil {
		log.Fatal(err)
	}
}

// ---------------- Serial ----------------

func (m model) Init() tea.Cmd {
	if m.connected {
		go func(port serial.Port, ch chan<- serialMsg) {
			buf := make([]byte, 256)
			for {
				n, err := port.Read(buf)
				if err == nil && n > 0 {
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

// ---------------- Parsing ----------------

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

func extractMessages(buf []byte) (msgs [][]byte, remaining []byte) {
	start := 0
	for {
		s := indexOf(buf[start:], SERIAL_START)
		if s == -1 {
			break
		}
		s += start

		e := indexOf(buf[s:], SERIAL_END)
		if e == -1 {
			break
		}
		e += s + len(SERIAL_END)

		msgs = append(msgs, buf[s:e])
		start = e
	}
	return msgs, buf[start:]
}

func removeAll(msg, sub []byte) []byte {
	for {
		i := indexOf(msg, sub)
		if i == -1 {
			break
		}
		msg = append(msg[:i], msg[i+len(sub):]...)
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

// Decode ASCII hex payload → text
func decodeHexText(msg []byte) string {
	s := strings.Fields(string(stripMarkers(msg)))
	if len(s) <= 3 {
		return ""
	}

	s = s[3:] // skip header bytes

	out := make([]byte, 0)
	for _, p := range s {
		if p == "00" {
			break
		}
		v, err := strconv.ParseUint(p, 16, 8)
		if err == nil {
			out = append(out, byte(v))
		}
	}
	return string(out)
}

func joinHex(b []byte) string {
	out := make([]string, len(b))
	for i, c := range b {
		out[i] = fmt.Sprintf("%02X", c)
	}
	return strings.Join(out, " ")
}

// ---------------- Update ----------------

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {

	case serialMsg:
		m.buffer = append(m.buffer, msg...)
		newMsgs, rem := extractMessages(m.buffer)
		m.buffer = rem

		for _, raw := range newMsgs {
			m.messages = append(m.messages, raw)
			m.messageCount++
		}

		// rebuild table rows ONCE
		rows := make([]table.Row, len(m.messages))
		for i, msg := range m.messages {
			clean := stripMarkers(msg)
			rows[i] = table.Row{
				joinHex(clean),
				decodeHexText(msg),
			}
		}
		m.table.SetRows(rows)

		return m, readSerialCmd(m.ch)

	case tea.KeyMsg:
		if msg.String() == "q" || msg.String() == "ctrl+c" {
			return m, tea.Quit
		}
		m.table, _ = m.table.Update(msg)

	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		m.table.SetWidth(msg.Width / 2)
		m.table.SetHeight(msg.Height - 3)
	}
	return m, nil
}

// ---------------- View ----------------

func (m model) View() string {
	detail := ""
	if len(m.messages) > 0 && m.table.Cursor() < len(m.messages) {
		raw := stripMarkers(m.messages[m.table.Cursor()])
		txt := decodeHexText(m.messages[m.table.Cursor()])
		detail = m.detailStyle.Width(m.width/2 - 2).Render(
			fmt.Sprintf("Full Message:\n%s\n\nTXT:\n%s", string(raw), txt),
		)
	}

	content := lipgloss.JoinHorizontal(lipgloss.Top, m.table.View(), detail)

	info := fmt.Sprintf(
		"Serial: %s | Sending message %d off to MeshWay servers...",
		map[bool]string{true: "Connected", false: "Disconnected"}[m.connected],
		m.messageCount,
	)

	return lipgloss.JoinVertical(
		lipgloss.Left,
		content,
		m.infoStyle.Width(m.width).Render(info),
	)
}
