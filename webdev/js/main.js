var ws ;
var el_timer;
var counters={};
var elapsed = 0;
var si7021_chart;
var sht10_chart;
var is_local = false;
var socksrv;
var term;
var	logs = true;
var urls = {
				sensors:'sensors',
				hb:"hb",
				reset:"reset",
				factory_reset:"factory_reset",
				config_form:"config_form",
				update:"update",
				spiffs:"spiffs",
				system:"system",
				config:"config",
				version:"version.json", 
				wifiscan:"wifiscan" 
			};
var data = {
	    labels: [],
	    datasets: [
		    {
		      label: "Temperature",
		      fillColor: "rgba(77,204,51,0.2)",
		      strokeColor: "rgba(77,204,51,1)",
		      pointColor: "rgba(77,204,51,1)",
		      pointStrokeColor: "#fff",
		      pointHighlightFill: "#fff",
		      pointHighlightStroke: "rgba(77,204,51,1)"
	      },
	      {
	        label: "Humidity",
	        fillColor: "rgba(64,149,191,0.2)",
	        strokeColor: "rgba(64,149,191,1)",
	        pointColor: "rgba(64,149,191,1)",
	        pointStrokeColor: "#fff",
	        pointHighlightFill: "#fff",
		     pointHighlightStroke: "rgba(64,149,191,1)"
		    }
	    ]
		};



function isJson(item) {
	if ((item.charAt(0)=='{' && item.slice(-1)=='}') || (item.charAt(0)=='[' && item.slice(-1)==']'))
		return true;
	else
		return false
}		

function ts() {
	var myDate = new Date();
	var ts='';
	if (myDate.getHours()<10)	ts+='0';
	ts+=myDate.getHours() + ':';
	if (myDate.getMinutes()<10)	ts+='0';
	ts+=myDate.getMinutes() + ':';
	if (myDate.getSeconds()<10)	ts+='0';
	ts+=myDate.getSeconds() + ' ';
	return ts;
}

function getActiveTab() {
	return ($('.nav-tabs .active > a').attr('href'));
}

function formatSize(bytes) {
  if (bytes < 1024)  return bytes+' Bytes';
  if (bytes < (1024*1024)) return (bytes/1024).toFixed(0)+' KB';
  if (bytes < (1024*1024*1024)) return (bytes/1024/1024).toFixed(0)+' MB';
  return (bytes/1024/1024/1024).toFixed(0)+' GB';
}

function rowStyle(row, index) {  
	//var classes=['active','success','info','warning','danger'];  
	var flags=parseInt(row.fl,10);  
	//if (flags & 0x80) return {classes:classes[4]};
	//if (flags & 0x02) return {classes:classes[3]};
	//if (flags & 0x08) return {classes:classes[1]};
	if (flags & 0x80) return {classes:'danger'};
	if (flags & 0x02) return {classes:'warning'};
	if (flags & 0x08) return {classes:'success'};
	return {}; 
} 

function labelFormatter(value, row) {  
	var flags=parseInt(row.fl,10);  
	
	if ( typeof counters[value]==='undefined') 
		counters[value]=1;  
	if ( flags & 0x88 ) 
		counters[value]++;  
	return value + ' <span class=\"badge\">'+counters[value]+'</span>';   
	}  

function fileFormatter(value, row) {  
	var fname = row.na;
	var htm = '';  
	//ht+= '<button type="button" class="btn btn-xs btn-danger" title="Supprimer">';
	//ht+= '<span class="glyphicon glyphicon-trash" aria-hidden="true"></span>&nbsp;';
	//ht+= '</button>&nbsp;';
	//ht+= '<button type="button" class="btn btn-xs btn-primary" title="Télécharger">';
	//ht+= '<span class="glyphicon glyphicon-trash" aria-hidden="true"></span>&nbsp;';
	//ht+= '</button>&nbsp;';
	htm+= '<a href="' + fname + '">' + fname + '</a>';

	return htm;
}  

function SSIDFormatter(value, row) {  
	var ssid=value;  
	var enc=String(row.enc);  
	var opn = (enc=='Open') ? true : false;  
	
	var cl, htm;  

	htm = "<div>";

	cl='success';  
	if (opn) 
		cl='eye-open';  
	else
		cl='lock';  

	//if (enc=='WPAOpen') cl='eye-open';  

	htm+= "<span class='glyphicon glyphicon-"+cl+"'></span>&nbsp;" + ssid;
	if (!opn)
		htm+= "&nbsp;<span class='badge label-"+cl+"'> "+enc+" </span> ";
	htm+= "</div>";

	return htm;
}  

function RSSIFormatter(value, row) {  
	var rssi=parseInt(row.rssi);  
	var signal = Math.min(Math.max(2*(rssi+100),0),100);
	var cl, htm;  

	cl='success';  
	if (signal<70) cl ='info';  
	if (signal<50) cl ='warning';  
	if (signal<30) cl ='danger';  
	cl = 'progress-bar-' + cl;  

	htm= "<div class='progress progress-tbl'>";
	htm+= "<div class='progress-bar "+cl+"' role='progressbar' aria-valuemin='0' aria-valuemax='100' ";
	htm+= "aria-valuenow='"+signal+"' style='width:"+signal+"%'>"+signal+"%</div></div>";

	return htm;
}  

function sliderFormatter(value, type, unit) {  
	if ( value.length == 2 ) {
		var txt ;
		var res ;
		var item = '#sens_';

		item += unit=='%'?'hum':'temp';
		item += '_'+type+'_help';
		item = $(item);

		//console.log('item='+item);

		if (type =='led') {
			txt = 'Green from ' + value[0] + unit + ' to ' + value[1] + unit ;
			$(item).text('Select analog output range for temperature from 0V to 10V. Current ' + txt);
			return txt;
		} 
	}
}  

function Notify(mydelay, myicon, mytype, mytitle, mymsg) {  
	$.notify(
		{	icon:'glyphicon glyphicon-'+myicon,
			title:'&nbsp;<strong>'+mytitle+'</strong>',
			message:'<p>'+mymsg+'</p>',
		},{
			type:mytype,
			z_index: 2048,
			placement:{ align:'center'},
			//showProgressbar: true,
			animate:{enter:'animated fadeInDown',exit:'animated fadeOutUp',delay:mydelay*1000}
		}
	);
}  

function progressUpload(data) {
  if(data.lengthComputable) {
  	var pe = (data.loaded/data.total*100).toFixed(0) ;
  	$('#pfw').css('width', pe +'%');
   	$('#psfw').text(formatSize(data.loaded)+' / '+formatSize(data.total));  
  }
}

function waitReboot() {
	var url = location.href ;
	$('#txt_srv').text('Trying to connect to '+url);
	$('#mdl_wait').modal();
	el_timer=setInterval(function(){elapsed++;$('#txt_elapsed').text('Time elapsed '+elapsed+' s');}, 1000);  
}

function refreshSensors(sensors_data) {
  //console.log('refreshSensors=' + data);
	var si7021;
	var sht10 ;
	var now = new Date();
	var hour = now.getHours();
	var min = now.getMinutes();
	var sec = now.getSeconds();

  if (typeof(' sensors_data.si7021')!='undefined')
		si7021 = sensors_data.si7021[0];
  if (typeof(' sensors_data.sht10')!='undefined')
		sht10 = sensors_data.sht10[0];

	
	var label=hour<10?'0'+hour:hour;
	label+=':';
	label+=min<10?'0'+min:min;
	label+=':';
	label+=sec<10?'0'+sec:sec;

	if (typeof(si7021)!='undefined') {
		$("#sp_si7021_temp").text(si7021.temperature+'°C');
		$("#sp_si7021_hum").text(si7021.humidity+'%');

		if (si7021_chart.datasets[0].points.length >= 20)
			si7021_chart.removeData()
		si7021_chart.addData([si7021.temperature,si7021.humidity],label);
		si7021_chart.update();
	} else {
		$("#sp_si7021_state").text('Not Found/Enabled');
	}

	if (typeof(sht10)!='undefined') {
		$("#sp_sht10_state").text('');
		$("#sp_sht10_temp").text(sht10.temperature+'°C');
		$("#sp_sht10_hum").text(sht10.humidity+'%');
		if (sht10_chart.datasets[0].points.length >= 20)
			sht10_chart.removeData()
		sht10_chart.addData([sht10.temperature,sht10.humidity],label);
		sht10_chart.update();
	} else {
		$("#sp_sht10_state").text('Not Found/Enabled');
	}
}

function refreshSystem(system_data) {
  console.log('refreshSystem=' + system_data);
	$('#tab_sys_data').bootstrapTable('load', system_data);  
}

function wsSend(message) {
	if (ws.readyState===1)
		ws.send( message );
}

function toHex(val, n) {
	var txt = "";
	var hex = (+val).toString(16).toUpperCase();
	for (i=hex.length; i<n; i++)
		txt += "0";
	return (txt+hex);
}


// We clicked on a tab 
$('a[data-toggle=\"tab\"]').on('shown.bs.tab', function (e) {  
	var target = $(e.target).attr("href")  
	console.log('activated tab' + target );  

  // IE10, Firefox, Chrome, etc.
	if (history.pushState) 
    window.history.pushState(null, null, target);
	else 
    window.location.hash = target;

	if (target=='#tab_sensor') {
		wsSend( '$sensors:' + $('#sensors_sld_rfh').slider('getValue') );
		//$('#sensors_sld_rfh').redraw();
		//$('#si7021_chart').redraw();
		//$('#sht10_chart').redraw();

	} else if (target=='#tab_sys') {
		//$('#tab_sys_data').bootstrapTable('refresh',{silent:true, url:urls['system']});  
		wsSend('$system');
	} else if (target=='#tab_log') {
		wsSend('$log');
	} else if (target=='#tab_fs') {
		wsSend('$spiffs');
		$.getJSON( urls['spiffs'], function(spiffs_data) { 
			var pb, pe, cl;  		
			total= spiffs_data.spiffs[0].Total; 
			used = spiffs_data.spiffs[0].Used; 
			freeram = spiffs_data.spiffs[0].Ram; 

			$('#tab_fs_data').bootstrapTable('load', spiffs_data.files, {silent:true, showLoading:true});  

			pe=parseInt(used*100/total);  
			if (isNaN(pe)) 
				pe=0;  
			cl='success';  
			if (pe>70) cl ='warning';  
			if (pe>85) cl ='danger';  

			cl = 'progress-bar-' + cl;  
			if (pe>0) 
				$('#sspiffs').text(pe+'%');  
			$('#fs_use').text(formatSize(used)+' / '+formatSize(total));  
			$('#pfsbar').attr('class','progress-bar '+cl);  
			$('#pfsbar').css('width', pe+'%');

		})
		.fail(function() { console.log( "error while requestiong SPIFFS data" );	})

		$.getJSON( urls['version'], function(spiffs_version) { 
			var v= "Web V" + spiffs_version.version + '.' + spiffs_version.build; 
			console.log('spiffs version='+v);
			$("#spiffs_version").html(v);
		})


	// Configuration Tab activation
	} else if (target=='#tab_cfg') {
		//socket.emit('config', true);
		wsSend('$config');
		$.getJSON( urls['config'], function(data) { 
			var sens_si7021 = data["sens_si7021"]==1?true:false;
			var sens_sht10 = data["sens_sht10"]==1?true:false;
			var cfg_ap = data["cfg_ap"]==1?true:false ;
			var cfg_wifi = data["cfg_wifi"]==1?true:false ;
			var cfg_debug = data["cfg_debug"]==1?true:false ;
			var cfg_rgb = data["cfg_rgb"]==1?true:false ;
			var cfg_oled = data["cfg_oled"]==1?true:false ;
			var cfg_static = data["cfg_static"]==1?true:false ;
			var sens_temp_led = JSON.parse("[" + data["sens_temp_led"] + "]");
			var sens_hum_led = JSON.parse("[" + data["sens_hum_led"] + "]");
			var cfg_led_bright= data["cfg_led_bright"] ;
			var cfg_led_hb = data["cfg_led_hb"] ;

			console.log('led='+cfg_led_bright+'% '+cfg_led_hb+'s');

			$("#frm_config").autofill(data); 

			$("#sens_temp_led").slider({ id:"slider_temp_led", value:sens_temp_led, formatter: function(v){return sliderFormatter(v,'led','°C');}  });
			$("#sens_hum_led").slider({ id:"slider_hum_led", value:sens_hum_led, formatter: function(v){return sliderFormatter(v,'led','%');}	});

			$("#cfg_led_bright").slider({ id:"slider_led_bright", value:cfg_led_bright, formatter: function(v){return v+'%';}	});
			$("#cfg_led_hb").slider({ id:"slider_led_hb", value:cfg_led_hb, formatter: function(v){return (v/10)+'s';}	});

			$("#sens_si7021").bootstrapSwitch('state', sens_si7021);
			$("#sens_sht10").bootstrapSwitch('state', sens_sht10);
			$("#cfg_ap").bootstrapSwitch('state', cfg_ap);
			$("#cfg_wifi").bootstrapSwitch('state', cfg_wifi);
			$("#cfg_debug").bootstrapSwitch('state', cfg_debug);
			$("#cfg_rgb").bootstrapSwitch('state', cfg_rgb);
			$("#cfg_oled").bootstrapSwitch('state', cfg_oled);
			$("#cfg_static").bootstrapSwitch('state', cfg_static);
		})
		.fail(function(xhr, textStatus, errorThrown) { 
				Notify(2, 'warning-sign', 'warning', 'Error while getting configuration', xhr.status+' '+errorThrown);
		})
		//$('#tab_scan_data').bootstrapTable('refresh',{silent:true, showLoading:true, url:'/wifiscan.json'});  
	}
});  

$('#tab_sys_data').on('load-success.bs.table', function (e, data) { console.log('#tab_sys_data loaded');  })
$('#tab_fs_data').on('load-success.bs.table', function (e, data) {  console.log('#tab_fs_data loaded');  })
.on('load-error.bs.table', function (e, status) {  	console.log('Event: load-error.bs.table');  });  

$('#tab_scan').on('click-row.bs.table', function (e, name, args) {  
	var $form = $('#tab_cfg');  
	$('#ssid').val(name.ssid);  
	setTimeout(function(){$('#psk').focus()},500);  
	$('#tab_scan').modal('hide');  
});  
$('#btn_scan').click(function () {  
	$('#tab_scan_data').bootstrapTable('refresh',{url:urls['wifiscan'],showLoading:false,silent:false});  
});  
$('#btn_reset').click(function () {  
	$.post(urls['factory_reset']);  
  waitReboot(); 
  return false;
});  
$('#btn_reboot').click(function () {  
  $.post(urls['reset']); 
  waitReboot(); 
  return false;
});  

$(document)
	.on('change', '.btn-file :file', function() {
	  var input = $(this),
  	numFiles = input.get(0).files ? input.get(0).files.length : 1,
  	label = input.val().replace(/\\/g, '/').replace(/.*\//, '');
		input.trigger('fileselect', [numFiles, label]);
	})
  .on('hide.bs.collapse', '.panel-collapse', function (e) {
      var $span = $(this).parents('.panel').find('span.pull-right.glyphicon');
      $span.removeClass('glyphicon-chevron-up').addClass('glyphicon-chevron-down');
  })
  .on('show.bs.collapse', '.panel-collapse', function (e) {
      var $span = $(this).parents('.panel').find('span.pull-right.glyphicon');
      $span.removeClass('glyphicon-chevron-down').addClass('glyphicon-chevron-up');
  })
  .on('shown.bs.collapse', '.panel-collapse', function (e) {
      if (e.currentTarget.id=='col_sht10') {
      	sht10_chart.resize();
      	sht10_chart.update();
      }
      else if (e.currentTarget.id=='col_si7021') {
      	si7021_chart.resize();
      	si7021_chart.update();
      }
  })
;

$('#frm_config').validator().on('submit', function (e) {
	// everything looks good!
	if (!e.isDefaultPrevented()) {
		var myForm = $("#frm_config").serialize();
		e.preventDefault();
		console.log("Form Submit="+myForm);
		$.post(urls['config_form'], $("#frm_config").serialize())
		 .done( function(msg, textStatus, xhr) {Notify(2, 'ok', 'success', 'Configuration saved', xhr.status+' '+msg);})
		 .fail( function(xhr, textStatus, errorThrown) {Notify(4, 'remove', 'danger', 'Error while saving configuration', xhr.status+' '+errorThrown);});
	}
});

$('#file_fw').change(function() {
	var $txt = $('#txt_upload_fw');
	var $btn = $('#btn_upload_fw');
	var ok = true;
	var f = this.files[0];
	var name = f.name.toLowerCase();
	var size = f.size;
	var type = f.type;
	var html = 'Fichier:' + name + '&nbsp;&nbsp;type:' + type + '&nbsp;&nbsp;taille:' + size + ' octets'
	console.log('name='+name);
	console.log('size'+size);
	console.log('type='+type);

	$('#pgfw').removeClass('show').addClass('hide');
	$('#sfw').text( name + ' : ');  

	if (!f.type.match('application/octet-stream')) {
		Notify(3, 'remove', 'danger', 'Bad file type', 'Updated file should be a binary file');
		ok = false;
	} else if (name!="nrjmeter.cpp.bin" && name!="nrjmeter.spiffs.bin") {
		Notify(5, 'remove', 'danger', 'Bad filename', 'Updated filename should be named <ul><li>nrjmeter.cpp.bin (Firmware) or</li><li>nrjmeter.spiffs.bin (SPIFFS Filesystem)</li></ul>');
		ok = false;
	}
	if (ok) {
		$btn.removeClass('hide');
		if ( name==="nrjmeter.cpp.bin") {
			label = 'Firmware Update';
		}	else {
			label = 'SPIFFS Update';
		}
		$btn.val(label);
		$('#fw_info').html('<strong>' + label + '</strong> ' + html);
	} else {
		$txt.attr('readonly', '');
		$txt.val('');
		$txt.attr('readonly', 'readonly');
		$btn.addClass('hide');
	}
	return ok;
});

$('#btn_upload_fw').click(function() {
  var formData = new FormData($('#frm_fw')[0]);
  $.ajax({
    url: urls['update'],  
    type: 'POST',
    data: formData,
    cache: false,
    contentType: false,
    processData: false,
    xhr: function() {  
      var myXhr = $.ajaxSettings.xhr();
      if(myXhr.upload)
        myXhr.upload.addEventListener('progress',progressUpload, false); 
      return myXhr;
    },
    beforeSend: function () { 
				$('#pgfw').removeClass('hide');
    },
		success: function(msg, textStatus, xhr) {	
			Notify(2, 'floppy-saved', 'success','Upgrade sent successfull', '<strong>'+xhr.status+'</strong> '+msg); 
			waitReboot();
		},
		error: function(xhr, textStatus, errorThrown) {
				$('#pfw').removeClass('progress-bar-success').addClass('progress-bar-danger');
			Notify(4, 'floppy-remove', 'danger', 'Error while upgradding file '+name,'<strong>'+xhr.status+'</strong> '+errorThrown);
		}
  });
});

function refreshPage() {
	var url = document.location.toString();  
	var tab = url.split('#')[1];
	if (typeof(tab)==='undefined') 
		tab='tab_sensor';
	//console.log('Tab activation='+tab);
	$('.nav-tabs a[href=#'+tab+']').tab('show');  
	$('a[data-toggle="tab"][href="' + tab + '"]').trigger("shown.bs.tab");
	console.log('refreshPage('+tab+')')
	//$('.nav-tabs a[href=#'+tab+']').on('shown', function(e){window.location.hash=e.target.hash;});   
}

window.onload = function() {  

  // Instanciate the terminal object
  term = $('#term').terminal( function(command, term) {
    	if (command == '!help') {
        this.echo("available commands are [[b;cyan;]!help], [[b;cyan;]!log], [[b;cyan;]!nolog]\n" + 
 									"each terminal commmand need to be prefixed by [[b;cyan;]!]\n" + 
 									"if not they will be send and interpreted by NRJMeter\n" +
                   "examples: [[b;cyan;]!nolog]  disable log output to this console\n" +
                  "          [[b;cyan;]!log]    enable log output to this console"); 

    	} else if (command == '!nolog') {
        this.echo("[[b;cyan;]Log disabled]");
    		logs = false;
    	} else if (command == '!log') {
        this.echo("[[b;cyan;]Log enabled]");
    		logs = true;
    	} else if (command == '!log?') {
        this.echo("[[b;cyan;]Log="+logs+"]");
    	} else {
    		// Send raw command to client
    		wsSend(command);
    	}
    }, 
    // Default terminal settings and greetings
    { prompt: '[[b;red;]>]', 
      checkArity : false,
      greetings: "Type [[b;cyan;]help] to see NRJMeter available commands\n" +
                 "Type [[b;cyan;]!help] to see terminal commands"
    }
  );

	// open a web socket
	socksrv = 'ws://'+location.hostname+':'+location.port+'/ws';
	console.log('socket server='+socksrv);
	//ws = new WebSocket("ws://localhost:8081");
	//ws = new WebSocket(socksrv);
	ws = new ReconnectingWebSocket(socksrv);
	//ws.debug = true;

  // When Web Socket is connected
	ws.onopen = function(evt)	{
		console.log( 'WS Connect');
		//Notify(1, 'eye-open', 'success', 'Connected to nrjmeter', '');
		//$('.nav-tabs a[href=#'+document.location.toString().split('#')[1]+']').tab('show');  
		$('#mdl_wait').modal('hide');	
		$('#connect').text('connected').removeClass('label-danger').addClass('label-success');	
		var srv = location.hostname;
		term.echo("WS[Connected] to [[;green;]ws:/"+srv+':'+location.port+"/ws]");
		term.set_prompt("[[b;green;]"+srv+" #]");
		elapsed=0;
		clearInterval(el_timer);
		refreshPage();
	};

	// When websocket is closed.
	ws.onclose = function(code, reason)	{ 
		$('#connect').text('disconnected').removeClass('label-success').addClass('label-danger');	
		term.error('WS[Disconnected] '+code+' '+reason);
		term.set_prompt("[[b;red;]>]");
	};

	// When websocket message
	ws.onmessage = function (evt) { 
		console.log( 'WS Received '+evt.data);
		if (isJson(evt.data)) {
			console.log( 'WS Received JSON');
			var obj =  eval( "(" + evt.data + ")" );
		  var msg = obj.message;
		  var data = obj.data;
		
		  if ( logs == true) {
		  	var str = ts()+"WS["+msg+"] "+ JSON.stringify(data);
		  	str = str.replace(/\]/g, "&#93;");
		  	term.echo("[[;darkgrey;]"+str+"]");
		  }
		  if (msg=='sensors')	
		  	refreshSensors(data);
		  if (msg=='system') 	
		  	refreshSystem(data);
		  if (msg=='log' && logs == true )	{
		  	data.replace(/\]/g, "&#93;");
		  	term.echo("[[;darkgrey;]"+ts()+data+"]");
		  }
		} else {
			// Raw Data (mainly response)
			console.log( 'WS Received RAW');
	  	var str = evt.data;
	  	str = str.replace(/\]/g, "&#93;");
	  	term.echo("[[;darkgrey;]"+str+"]");
		}
	};

  // When Web Socket error
	ws.onerror = function(evt)	{
		term.error( ts()+'WS[error]');
	};

	$("#sensors_sld_rfh")
		.slider({ id: "si7021_slider_rfh", formatter:function(v){ return v+' seconds';}	})
		.on('slideStop',function(){	wsSend('$sensors:'+$('#sensors_sld_rfh').slider('getValue'));});

	$("#cfg_led_bright")
		.on('slideStop',function(){	wsSend('$rgbb:'+$('#cfg_led_bright').slider('getValue'));});

	$("#sens_si7021").bootstrapSwitch({onColor:"success", offColor:"danger"});
	$("#sens_sht10").bootstrapSwitch({ onColor: "success", offColor:"danger"});
	$("#cfg_debug").bootstrapSwitch({ onColor:"success", offColor:"danger"});
	$("#cfg_rgb").bootstrapSwitch({ onColor:"success", offColor:"danger"});
	$("#cfg_oled").bootstrapSwitch({ onColor:"success", offColor:"danger"});
	$("#cfg_ap").bootstrapSwitch({ onColor:"success", offColor:"danger"});
	$("#cfg_wifi").bootstrapSwitch({ onColor:"success", offColor:"danger"});
	$("#cfg_static").bootstrapSwitch({ onColor:"success", offColor:"danger"});

	// enable chart
	var ctx_si = $("#si7021_chart").get(0).getContext("2d");
	si7021_chart = new Chart(ctx_si).Line(data, {responsive: true});

	var ctx_sh = $("#sht10_chart").get(0).getContext("2d");
	sht10_chart = new Chart(ctx_sh).Line(data, {responsive: true});
	//refreshSensors();
	//refreshPage();
}

