document.getElementById('noise-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const payload = {
    rate: parseInt(document.getElementById('rate').value, 10),
    channels: parseInt(document.getElementById('channels').value, 10),
    duration_ms: parseInt(document.getElementById('duration').value, 10),
    amp: parseFloat(document.getElementById('amp').value)
  };
  try {
    const res = await fetch('/audio/whitenoise', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const text = await res.text();
    document.getElementById('noise-result').innerHTML = text;
  } catch (err) {
    document.getElementById('noise-result').innerHTML = '<small>Failed to start noise.</small>';
  }
});

document.getElementById('noise-stop').addEventListener('click', async () => {
  try {
    const res = await fetch('/audio/whitenoise/stop', { method: 'POST' });
    const text = await res.text();
    document.getElementById('noise-result').innerHTML = text;
  } catch (err) {
    document.getElementById('noise-result').innerHTML = '<small>Failed to stop noise.</small>';
  }
});
