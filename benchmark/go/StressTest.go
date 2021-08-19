package main

import (
	"fmt"
	"os"
	"runtime"
	"strconv"
	"time"
)

type SimpleTimer struct {
	t1 int64
	t2 int64
}

func (t *SimpleTimer) reset() int64 {
	t.t1 = t.t2
	t.t2 = time.Now().UnixNano() / 1000000

	elapsed := t.t2 - t.t1
	return elapsed
}

func newTimer() SimpleTimer {
	t := SimpleTimer{}
	t.reset()
	t.t1 = t.t2
	return t
}

const (
	_1K = int(1024)
	_1M = _1K * _1K
)

var (
	g_testNodes []*BaggageNode
	g_testTitle string
)

type StressTestOptions struct {
	basePayload        int
	primitiveBytes     int
	circularRefPercent int
	replacePercent     int
}

type StressTest struct {
	_cntAllocatedNodes int
	_options           *StressTestOptions
}

type Node struct {
	_id        int32
	var1       *Node
	var2       *Node
	var3       *Node
	parent     *Node
	root       *Node
	primitives []int8
}

type BaggageNode struct {
	Node
	_cntNodes       int
	_cntCircularRef int
	_test           *StressTest
}

func newNode(test *StressTest, parent *Node) *Node {
	node := &Node{}
	node.initNode(test, parent)
	return node
}

func (self *Node) initNode(test *StressTest, parent *Node) {
	self._id = int32(test._cntAllocatedNodes)
	test._cntAllocatedNodes++
	self.primitives = make([]int8, test._options.primitiveBytes)
	if parent != nil {
		self.parent = parent
		self.root = parent.root
	}
}

func (self *BaggageNode) initBaggageNode(test *StressTest, cntNodes int) {
	self.initNode(test, nil)
	self._test = test
	self._cntNodes = cntNodes
	self._cntCircularRef = test._options.circularRefPercent * cntNodes / 100
	self.root = &self.Node
	self.attachChildren(&self.Node, 0)
}

func (self *BaggageNode) attachChildren(node *Node, layer int) {
	node.var1 = self.createChildNode(node, layer)
	node.var2 = self.createChildNode(node, layer)
	node.var3 = self.createChildNode(node, layer)
}

func (self *BaggageNode) createChildNode(parent *Node, layer int) *Node {
	idx_child := self._test._cntAllocatedNodes - int(self._id)
	if idx_child >= self._cntNodes {
		return nil
	}

	if idx_child >= self._cntCircularRef {
		parent = nil
	}
	node := newNode(self._test, parent)
	layer++
	if layer < 7 {
		self.attachChildren(node, layer)
	}
	return node
}

func (self *StressTest) init(options *StressTestOptions) {
	self._cntAllocatedNodes = 0
	self._options = options
}

func (self *StressTest) createBaggage(cntNodes int) *BaggageNode {
	if cntNodes == 0 {
		return nil
	}
	root := &BaggageNode{}
	root.initBaggageNode(self, cntNodes)
	return root
}

func (self *StressTest) stressTest(payload int) int64 {
	defer onOOM()

	fmt.Println("--------------------------------------")
	fmt.Println("Preparing test...")

	baggages := make([]*BaggageNode, payload)
	g_testNodes = baggages
	self._cntAllocatedNodes = 0
	runtime.GC()
	runtime.GC()

	for i := int(0); i < payload; i++ {
		baggages[i] = self.createBaggage(_1K)
	}

	timer := newTimer()
	timer2 := newTimer()

	fmt.Println(g_testTitle + " StressTest started. " +
		"nodes: " + strconv.FormatInt(int64(self._cntAllocatedNodes)/1024, 10) + "k")

	timer.reset()
	timer2.reset()
	updateLimit := payload * self._options.replacePercent / 100
	if updateLimit == 0 {
		updateLimit = 1
	}
	for repeat := 0; repeat < 5; repeat++ {
		for i := int(0); i < payload; i++ {
			idx := i % updateLimit
			baggages[idx] = self.createBaggage(_1K)
		}

		fmt.Println(strconv.Itoa(repeat+1) + ") " + strconv.FormatInt(timer.reset(), 10) + " ms")
	}

	baggages = nil
	elapsed := timer2.reset()
	fmt.Println("Total elapsed time: " + strconv.FormatInt(elapsed, 10))
	return elapsed
}

func main() {
	options := &StressTestOptions{}
	options.basePayload, _ = strconv.Atoi(os.Args[1])
	options.primitiveBytes, _ = strconv.Atoi(os.Args[2])
	options.circularRefPercent, _ = strconv.Atoi(os.Args[3])
	options.replacePercent, _ = strconv.Atoi(os.Args[4])

	g_testTitle = "[" + runtime.Version() + "]"

	fmt.Println(g_testTitle + " StressTest")
	fmt.Println(" - Base payload: " + strconv.Itoa(options.basePayload) + "k nodes")
	fmt.Println(" - Primitive bytes: " + strconv.Itoa(options.primitiveBytes) + " byte")
	fmt.Println(" - Circular ref percent: " + strconv.Itoa(options.circularRefPercent) + "%")
	fmt.Println(" - Replace percent: " + strconv.Itoa(options.replacePercent) + "%")

	test := &StressTest{}
	test.init(options)

	payload := 0
	print(g_testTitle)
	for {
		payload += int(options.basePayload)
		elapsed := test.stressTest(payload)
		print(", " + strconv.Itoa(int(elapsed)))
	}
}

func onOOM() {
	if err := recover(); err != nil {
		fmt.Println("termnated by OutOfMemoryError")
	}
}
