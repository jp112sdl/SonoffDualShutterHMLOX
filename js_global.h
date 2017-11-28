const char HTTP_CUSTOMSCRIPT[] PROGMEM = R"=====(

function Get(u){ 
  var h = new XMLHttpRequest(); 
  h.open('GET',u,false); 
  h.send(null); 
  return h.responseText; 
} 

function SetState(v) { 
  document.getElementById('shutter_value').value = ''; 
  var json_obj = JSON.parse(Get(v)); 
  refreshState(json_obj, false); 
} 

function isInt(v){
  return !isNaN(v) && parseInt(Number(v))==v && !isNaN(parseInt(v,10));
}

function refreshState(json_obj, rekursiv) { 
  if (document.getElementById('_ls') != null) {
   if (json_obj == null) json_obj = JSON.parse(Get('/getState')); 
   if (json_obj.state) document.getElementById('_ls').innerHTML = json_obj.state + "%"; 
  }
  if (rekursiv) setTimeout(function(){ refreshState(null, true); }, 3000); 
} 

/*init refresh:*/ 
refreshState(null, true); 

)=====";
