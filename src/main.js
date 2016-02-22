inlets = 1;
outlets = 4;

	var buf = new Buffer("som");
	var a;

	var myrepository = new Global("my_global_repository");
	var x = 'coordX';
	var y = 'coordY';

function bang(){

var len ;
len = buf.length();
a = new Array();
a = list(0, 500);
post(a);
outlet(3, a );
return 1;
}

function ze(){
post('aqueloutro');
	return 1;

}


function setvalue()
{	
	post('x: ' + arguments[0] + '  y: ' + arguments[1] );
	myrepository[x] = arguments[0];
	myrepository[y] = arguments[1];
	
	outlet(0, myrepository[x] );
	outlet(1, myrepository[y] );
	var message = 'ACK';
	outlet(2, message);
	return 1;
}

function list(index, count)
{
	var samples = buf.peek(1, index, count);
	return samples;
}