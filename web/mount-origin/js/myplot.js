
var counter = 0;


var chartdata = [40, 60, 80, 100, 70, 120, 100, 60, 70, 150, 120, 140];    
//  the size of the overall svg element
var height = 200;
var width = 720;
//  the width of each bar and the offset between each bar
var barWidth = 40;
var barOffset = 20;
d3.select('#bar-chart').append('svg')
    .attr('width', width)
    .attr('height', height)
    .style('background', '#dff0d8')
    .selectAll('rect').data(chartdata)
    .enter().append('rect')
    .style({'fill': '#3c763d', 'stroke': '#d6e9c6', 'stroke-width': '5'})
    .attr('width', barWidth)
    .attr('height', function (data) {
        return data;
    })
    .attr('x', function (data, i) {
        return i * (barWidth + barOffset);
    })
    .attr('y', function (data) {
        return height - data;
    });



var data_sin_inc = d3.range(40).map(function(i) {
    return i % 5 ? {x: i / 39, y: (Math.sin(i / 3) + 2) / 4} : null;
});

var dv = new Float64Array([1, 2, 3]);

// d3.select("body")
// .selectAll("p")
// .data([4, 8, 15, 16, 23, 42])
// .enter().append("p")
// .text(function(d) { return "I’m number " + d + "!"; });


// Update…
var p = d3.select("body")
    .selectAll("p")
    .data([4, 8, 15, 16, 23, 42])
    .text(function(d) { return d; });

// Enter…
p.enter().append("p")
    .text(function(d) { return d; });

// Exit…
p.exit().remove();



/////////////////////
// Set the dimensions of the canvas / graph
var margin = {top: 30, right: 20, bottom: 30, left: 50},
    width = 600 - margin.left - margin.right,
    height = 270 - margin.top - margin.bottom;

// Parse the date / time
var parseDate = d3.time.format("%d-%b-%y").parse;

// Set the ranges
var x = d3.time.scale().range([0, width]);
var y = d3.scale.linear().range([height, 0]);

// Define the axes
var xAxis = d3.svg.axis().scale(x)
    .orient("bottom").ticks(5);

var yAxis = d3.svg.axis().scale(y)
    .orient("left").ticks(5);

// Define the line
var valueline = d3.svg.line()
    .x(function(d) { return x(d.date); })
    .y(function(d) { return y(d.close); });

// Adds the svg canvas
var svg = d3.select("body")
    .append("svg")
    .attr("width", width + margin.left + margin.right)
    .attr("height", height + margin.top + margin.bottom)
    .append("g")
    .attr("transform", 
          "translate(" + margin.left + "," + margin.top + ")");

// Get the data
d3.csv("data.csv", function(error, data) {
    data.forEach(function(d) {
        d.date = parseDate(d.date);
        d.close = +d.close;
    });

    // Scale the range of the data
    x.domain(d3.extent(data, function(d) { return d.date; }));
    y.domain([0, d3.max(data, function(d) { return d.close; })]);

    // Add the valueline path.
    svg.append("path")
        .attr("class", "line")
        .attr("d", valueline(data));

    // Add the X Axis
    svg.append("g")
        .attr("class", "x axis")
        .attr("transform", "translate(0," + height + ")")
        .call(xAxis);

    // Add the Y Axis
    svg.append("g")
        .attr("class", "y axis")
        .call(yAxis);
});

var inter = setInterval(function() {
    counter += 1;
    document.getElementById("demo").innerHTML = counter;
    if (counter%2 == 0){
        updateData_1()   
    }
    else{
        updateData();
    }
}, 1000); 

// ** Update data section (Called from the onclick)
function updateData() {
    // Get the data again
    d3.csv("data-alt.csv", function(error, data) {
       	data.forEach(function(d) {
	    d.date = parseDate(d.date);
	    d.close = +d.close;
	});

    	// Scale the range of the data again 
    	x.domain(d3.extent(data, function(d) { return d.date; }));
	y.domain([0, d3.max(data, function(d) { return d.close; })]);

        // Select the section we want to apply our changes to
        var svg = d3.select("body").transition();

        // Make the changes
        svg.select(".line")   // change the line
            .duration(750)
            .attr("d", valueline(data));
        svg.select(".x.axis") // change the x axis
            .duration(750)
            .call(xAxis);
        svg.select(".y.axis") // change the y axis
            .duration(750)
            .call(yAxis);
    });
}


function updateData_1() {
    d3.csv("data.csv", function(error, data) {
       	data.forEach(function(d) {
	    d.date = parseDate(d.date);
	    d.close = +d.close;
	});

    	// Scale the range of the data again 
    	x.domain(d3.extent(data, function(d) { return d.date; }));
	y.domain([0, d3.max(data, function(d) { return d.close; })]);

        // Select the section we want to apply our changes to
        var svg = d3.select("body").transition();

        // Make the changes
        svg.select(".line")   // change the line
            .attr("d", valueline(data));
        svg.select(".x.axis") // change the x axis
            .call(xAxis);
        svg.select(".y.axis") // change the y axis
            .call(yAxis);
    });

}

