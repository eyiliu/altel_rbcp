
function DOMContentLoadedListener_colorblocks() {
    // settings for a grid with 40 cells in a row and 2x5 cells in a group
    var cellNumberX = 256;
    var cellNumberY = 128;
    var cellNumber = cellNumberX * cellNumberY;
    var groupCellNumberX = 8;
    var groupCellNumberY = 128;
    var groupNumberX = Math.ceil(cellNumberX / groupCellNumberX);
    var groupNumberY = Math.ceil(cellNumberY / groupCellNumberY)
    
    var groupSpacingX = 3;
    var groupSpacingY = 1;
    var cellSpacing = 1;
    var cellSize = 3;
    
    var width =  groupSpacingX * (groupNumberX +1 ) + (cellSize + cellSpacing) * cellNumberX;
    var height = groupSpacingY * (groupNumberY +1 ) + (cellSize + cellSpacing) * cellNumberY;    
    
    var data = [];
    var data_n = d3.range(0, cellNumber, 1);
    data_n.forEach( function(d) { data.push({ value: d, x: d/10});}); 
    
    var mainCanvas = d3.select('#container')
        .append('canvas')
        .classed('mainCanvas', true)
        .attr('width', width)
        .attr('height', height);

    // === Bind data to custom elements === //
    var customBase = document.createElement('custom');
    var custom = d3.select(customBase); // this is our svg replacement
    databind(data);
    var t = d3.timer(function(elapsed) {draw(mainCanvas);if (elapsed > 300) t.stop();});
    

    // === Bind and draw functions === //
    function databind(data) {
        // var colorScale = d3.scaleSequential(d3.interpolateSpectral).domain(d3.extent(data, function(d) { return d.value; })); 
        var colorScale = d3.scaleSequential()
            .domain([0, cellNumber])
            .interpolator(d3.interpolateRainbow);

        var join = custom.selectAll('custom.rect').data(data);
        var enterSel = join.enter()
	    .append('custom')
	    .attr('class', 'rect')
	    .attr('x', function(d, i) {
                var x1 = Math.floor(i % cellNumberX);
                var xg = Math.floor(x1 / groupCellNumberX);
	        return (cellSpacing + cellSize) * x1 + groupSpacingX * (xg+1);
	    })
	    .attr('y', function(d, i) {
                var y1 = Math.floor(i / cellNumberX);
                var yg = Math.floor(y1 / groupCellNumberY);
	        return (cellSpacing + cellSize) * y1 + groupSpacingY * (yg+1);
	    })
	    .attr('width', 0)
	    .attr('height', 0);

        join.merge(enterSel)
	    .transition()
	    .attr('width', cellSize)
	    .attr('height', cellSize)
	    .attr('fillStyle', function(d) { return colorScale(d.value); })



        var exitSel = join.exit()
	    .transition()
	    .attr('width', 0)
	    .attr('height', 0)
	    .remove();

    } // databind()

    function draw(canvas) { // <---- new arguments
        var context = canvas.node().getContext('2d');
        context.clearRect(0, 0, width, height);
        var elements = custom.selectAll('custom.rect') // this is the same as the join variable, but used here to draw
        elements.each(function(d,i) { // for each virtual/custom element...
	    var node = d3.select(this);
	    context.fillStyle =  node.attr('fillStyle'); // <--- new: node colour depends on the canvas we draw 
	    context.fillRect(node.attr('x'), node.attr('y'), node.attr('width'), node.attr('height'))
        });
    }
}




//////////////////////////////////////////////////////


var head = 0, tail = 0, ring = new Array();

function get_appropriate_ws_url(extra_url)
{
    var pcol;
    var u = document.URL;

    /*
     * We open the websocket encrypted if this page came on an
     * https:// url itself, otherwise unencrypted
     */

    if (u.substring(0, 5) === "https") {
	pcol = "wss://";
	u = u.substr(8);
    } else {
	pcol = "ws://";
	if (u.substring(0, 4) === "http")
	    u = u.substr(7);
    }

    u = u.split("/");

    /* + "/xxx" bit is for IE10 workaround */

    return pcol + u[0] + "/" + extra_url;
}

function new_ws(urlpath, protocol)
{
    if (typeof MozWebSocket != "undefined")
	return new MozWebSocket(urlpath, protocol);

    return new WebSocket(urlpath, protocol);
}


function DOMContentLoadedListener() {
    var n, wsa = new Array, alive = 0;
    
    for (n = 0; n < 1; n++) {
	
	ws = new_ws(get_appropriate_ws_url(""), "lws-minimal");
	wsa.push(ws);
	try {
	    ws.onopen = function() {
		document.getElementById("r").disabled = 0;
		alive++;
	    };

	    ws.onmessage = function got_packet(msg) {
		var n, s = "";
		
		ring[head] = msg.data + "\n";
		head = (head + 1) % 50;
		if (tail === head)
		    tail = (tail + 1) % 50;
		
		n = tail;
		do {
		    s = s + ring[n];
		    n = (n + 1) % 50;
		} while (n !== head);
		
		document.getElementById("r").value = s; 
		document.getElementById("r").scrollTop =
		    document.getElementById("r").scrollHeight;
	    };

	    ws.onclose = function(){
		if (--alive === 0)
		    document.getElementById("r").disabled = 1;
	    };

            
            function sendmsg()
	    {
		ws.send(document.getElementById("m").value);
		document.getElementById("m").value = "";
	    }
	    
	    document.getElementById("b").addEventListener("click", sendmsg);
            
            
	} catch(exception) {
	    alert("<p>Error " + exception);  
	}

        
    }
}


document.addEventListener("DOMContentLoaded", DOMContentLoadedListener, false);
document.addEventListener("DOMContentLoaded", DOMContentLoadedListener_colorblocks, false);
