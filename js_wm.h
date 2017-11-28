const char HTTP_SCRIPT[] PROGMEM = R"=====(
<script>
  function c(l) { 
    document.getElementById('s').value = l.innerText || l.textContent; document.getElementById('p').focus();
  }
  
  function setBackendType() {
    var backendtype = document.getElementById('backendtype'); 
    var ccu = document.getElementById('ccu'); 
    var qsa = document.querySelectorAll('[id^=div_]'); 
    qsa.forEach(function(e) { 
      e.style.display = 'block'; 
      }); 
      
        
    if (backendtype) { 
      var flt; 
      switch (parseInt(backendtype.value)) { 
        case 0: 
          flt = 'lox'; 
          if (ccu) ccu.placeholder = 'IP der CCU2'; 
          break; 
        case 1: 
          flt = 'hm'; 
          if (ccu) ccu.placeholder = 'IP des LOXONE Miniservers'; 
          break; 
      } 
      qsa = document.querySelectorAll('[id^=div_' + flt + ']'); 
      qsa.forEach(function(e) { e.style.display = 'none'; }); 
    } 
  }
</script>
)=====";

