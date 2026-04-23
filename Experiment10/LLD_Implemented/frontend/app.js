const API = 'http://localhost:8080/v1';

async function fetchRestaurants(){
  const res = await fetch(`${API}/restaurants`);
  return res.json();
}

async function fetchMenu(id){
  const res = await fetch(`${API}/restaurants/${id}/menu`);
  return res.json();
}

async function createOrder(payload, idempotencyKey){
  const res = await fetch(`${API}/orders`,{
    method:'POST',
    headers: {
      'Content-Type':'application/json',
      'Idempotency-Key': idempotencyKey || ''
    },
    body: JSON.stringify(payload)
  });
  return res.json();
}

function el(id){return document.getElementById(id)}

document.addEventListener('DOMContentLoaded', async ()=>{
  const list = el('list');
  const restaurants = await fetchRestaurants();
  restaurants.forEach(r=>{
    const li = document.createElement('li');
    li.innerHTML = `<strong>${r.name}</strong> — Rating: ${r.rating} <button data-id="${r.id}">View Menu</button>`;
    list.appendChild(li);
  });

  list.addEventListener('click', async (e)=>{
    if(e.target.tagName === 'BUTTON'){
      const id = e.target.dataset.id;
      const menu = await fetchMenu(id);
      showMenu(id, menu);
    }
  });

  el('back').addEventListener('click', ()=>{
    el('menu').hidden = true; el('restaurants').hidden = false; el('order-result').hidden = true;
  });
});

function showMenu(rid, menu){
  el('restaurants').hidden = true; el('menu').hidden = false; el('order-result').hidden = true;
  el('menu-title').textContent = `Menu — ${rid}`;
  const ml = el('menu-list'); ml.innerHTML = '';
  menu.items.forEach(item=>{
    const li = document.createElement('li');
    li.innerHTML = `<strong>${item.name}</strong> — ₹${(item.price/100).toFixed(2)} <button data-id="${item.id}" data-price="${item.price}">Order 1</button>`;
    ml.appendChild(li);
  });

  ml.addEventListener('click', async (e)=>{
    if(e.target.tagName === 'BUTTON'){
      const itemId = e.target.dataset.id;
      const price = parseInt(e.target.dataset.price,10);
      const payload = {
        user_id: 'demo_user',
        restaurant_id: rid,
        items: [{menu_item_id: itemId, quantity: 1, price}],
        payment_method: 'CARD'
      };
      const idem = 'demo-' + itemId; // simple idempotency for demo
      const res = await createOrder(payload, idem);
      el('result').textContent = JSON.stringify(res, null, 2);
      el('order-result').hidden = false;
    }
  });
}
