const cvs = document.getElementById('chart');
if (cvs) {
  const ctx = cvs.getContext('2d');
  cvs.width = cvs.clientWidth; cvs.height = 200;
  let data = [];
  window.pushChart = (v) => {
    data.push(v); if (data.length > 100) data.shift();
    draw();
  };
  function draw() {
    ctx.clearRect(0,0,cvs.width,cvs.height);
    if (data.length < 2) return;
    ctx.strokeStyle = '#f66'; ctx.beginPath();
    data.forEach((v,i)=>{
      const x = i * cvs.width / data.length;
      const y = cvs.height - v/120 * cvs.height;
      i ? ctx.lineTo(x,y) : ctx.moveTo(x,y);
    });
    ctx.stroke();
  }
}