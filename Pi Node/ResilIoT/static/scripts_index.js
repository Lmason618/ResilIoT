async function loadPage(fragment, scriptFile = null) {
    try {
        // Load frag
        const res = await fetch(fragment);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const html = await res.text();

        const main = document.getElementById('main-content');
        main.innerHTML = html;
        main.dataset.current = fragment; // which fragment is loaded

        // Dynamically load scripts.js for home.html
        if (scriptFile) {
            if (!window.initHome) {
                const existingScript = document.getElementById(scriptFile);
                if (!existingScript) {
                    const script = document.createElement('script');
                    script.id = scriptFile; // avoid duplicates
                    script.src = `/static/${scriptFile}`;
                    script.onload = () => window.initHome?.(); // call init after load
                    document.body.appendChild(script);
                }
            } else {
                window.initHome?.(); // already loaded, just call init
            }
        }
    } catch (err) {
        main.innerHTML = '<p class="text-red-600">Failed to load content.</p>';
        console.error(err);
    }
}

// Sidebar buttons
document.addEventListener('DOMContentLoaded', () => {
    const btnHome = document.querySelector('.btn-home');
    const btnAlert = document.querySelector('.btn-alert');

    btnHome?.addEventListener('click', () => loadPage('home.html', 'scripts.js'));
    btnAlert?.addEventListener('click', () => loadPage('params.html'));

    // Check URL query sends the user back to params.html on save
    const params = new URLSearchParams(window.location.search);
    if (params.get("open") === "params") {
        loadPage('params.html');
    } else {
        loadPage('home.html', 'scripts.js');
    }
});

