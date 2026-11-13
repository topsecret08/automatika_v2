function loadRecipes() {
  fetch('/api/recipes').then(r=>r.json()).then(list=>{
    const el = document.getElementById('recipes');
    if (!el) return;
    el.innerHTML = list.map(r=>`<div class="card">${r.name}</div>`).join('');
  }).catch(()=>{});
}