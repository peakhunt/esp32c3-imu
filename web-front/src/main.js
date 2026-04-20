import "bulma/css/bulma.min.css"
import { createApp } from "vue";
import { Icon } from '@iconify/vue'
import App from "./App.vue";
import './assets/hardcore.css'
import { addIcon } from '@iconify/vue'
import menuIcon from '@iconify-icons/mdi/menu'

addIcon('mdi:menu', menuIcon)

const app = createApp(App);

// Register the Icon component globally so you can use <Icon icon="mdi:gauge" /> anywhere
app.component('Icon', Icon);

app.mount("#app");
