const root = document.body;
const themeToggle = document.querySelector("[data-theme-toggle]");
const modal = document.querySelector("[data-modal]");
const filterButtons = document.querySelectorAll("[data-filter]");
const featureCards = document.querySelectorAll(".feature-card");
const searchInput = document.querySelector("[data-search]");
const emptyState = document.querySelector("[data-empty-state]");
const tabButtons = document.querySelectorAll("[data-tab-target]");
const tabPanels = document.querySelectorAll("[data-tab-panel]");
const billingToggle = document.querySelector("[data-billing-toggle]");
const priceValues = document.querySelectorAll(".price");
const priceCaptions = document.querySelectorAll("[data-price-caption]");
const faqButtons = document.querySelectorAll(".faq-question");
const toastRegion = document.querySelector("[data-toast-region]");
const counterElements = document.querySelectorAll("[data-counter]");

let activeFilter = "all";

const storedTheme = window.localStorage.getItem("ui-spark-theme");
if (storedTheme === "light" || storedTheme === "dark") {
    root.dataset.theme = storedTheme;
    themeToggle?.setAttribute("aria-pressed", String(storedTheme === "light"));
}

function showToast(message) {
    if (!toastRegion) {
        return;
    }

    const toast = document.createElement("div");
    toast.className = "toast";
    toast.textContent = message;
    toastRegion.appendChild(toast);

    window.setTimeout(() => {
        toast.remove();
    }, 3200);
}

function applyFeatureFilters() {
    const query = (searchInput?.value || "").trim().toLowerCase();
    let visibleCount = 0;

    featureCards.forEach((card) => {
        const category = card.getAttribute("data-category") || "";
        const name = (card.getAttribute("data-name") || "").toLowerCase();
        const matchesFilter = activeFilter === "all" || category === activeFilter;
        const matchesQuery = name.includes(query);
        const isVisible = matchesFilter && matchesQuery;

        card.classList.toggle("is-hidden", !isVisible);
        if (isVisible) {
            visibleCount += 1;
        }
    });

    emptyState?.classList.toggle("hidden", visibleCount !== 0);
}

function setTheme(theme) {
    root.dataset.theme = theme;
    window.localStorage.setItem("ui-spark-theme", theme);
    themeToggle?.setAttribute("aria-pressed", String(theme === "light"));
    showToast(`Theme updated to ${theme} mode.`);
}

themeToggle?.addEventListener("click", () => {
    const nextTheme = root.dataset.theme === "light" ? "dark" : "light";
    setTheme(nextTheme);
});

document.querySelectorAll("[data-toast-trigger]").forEach((button) => {
    button.addEventListener("click", () => {
        showToast(button.getAttribute("data-toast-trigger") || "Action completed.");
    });
});

document.querySelector("[data-open-modal]")?.addEventListener("click", () => {
    modal?.classList.remove("hidden");
    modal?.setAttribute("aria-hidden", "false");
});

document.querySelectorAll("[data-close-modal]").forEach((element) => {
    element.addEventListener("click", () => {
        modal?.classList.add("hidden");
        modal?.setAttribute("aria-hidden", "true");
    });
});

window.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
        modal?.classList.add("hidden");
        modal?.setAttribute("aria-hidden", "true");
    }
});

filterButtons.forEach((button) => {
    button.addEventListener("click", () => {
        activeFilter = button.getAttribute("data-filter") || "all";
        filterButtons.forEach((item) => item.classList.toggle("is-active", item === button));
        applyFeatureFilters();
    });
});

searchInput?.addEventListener("input", applyFeatureFilters);

tabButtons.forEach((button) => {
    button.addEventListener("click", () => {
        const target = button.getAttribute("data-tab-target");

        tabButtons.forEach((item) => {
            const isActive = item === button;
            item.classList.toggle("is-active", isActive);
            item.setAttribute("aria-selected", String(isActive));
        });

        tabPanels.forEach((panel) => {
            const isActive = panel.getAttribute("data-tab-panel") === target;
            panel.classList.toggle("is-active", isActive);
            panel.hidden = !isActive;
        });
    });
});

billingToggle?.addEventListener("change", () => {
    const yearly = billingToggle.checked;

    priceValues.forEach((price) => {
        price.textContent = yearly ? price.dataset.yearly || "" : price.dataset.monthly || "";
    });

    priceCaptions.forEach((caption) => {
        caption.textContent = yearly ? "per month, billed yearly" : "per month";
    });

    showToast(yearly ? "Yearly pricing enabled." : "Monthly pricing enabled.");
});

faqButtons.forEach((button) => {
    button.addEventListener("click", () => {
        const answer = button.nextElementSibling;
        const isExpanded = button.getAttribute("aria-expanded") === "true";

        button.setAttribute("aria-expanded", String(!isExpanded));
        if (answer instanceof HTMLElement) {
            answer.hidden = isExpanded;
        }
    });
});

function animateCounter(element) {
    const target = Number(element.getAttribute("data-counter") || "0");
    const duration = 900;
    const startTime = performance.now();

    function update(now) {
        const progress = Math.min((now - startTime) / duration, 1);
        element.textContent = String(Math.round(progress * target));

        if (progress < 1) {
            window.requestAnimationFrame(update);
        }
    }

    window.requestAnimationFrame(update);
}

const observer = new IntersectionObserver((entries, currentObserver) => {
    entries.forEach((entry) => {
        if (entry.isIntersecting) {
            animateCounter(entry.target);
            currentObserver.unobserve(entry.target);
        }
    });
}, { threshold: 0.6 });

counterElements.forEach((counter) => observer.observe(counter));

applyFeatureFilters();
