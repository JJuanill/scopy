!function(e,t){"object"==typeof exports&&"undefined"!=typeof module?module.exports=t():"function"==typeof define&&define.amd?define(t):(e="undefined"!=typeof globalThis?globalThis:e||self).Extra=t()}(this,(function(){"use strict";class e{constructor(e,t){if(this.$,"string"==typeof e){if(this.$=document.createElement(e),"object"==typeof t)for(const e in t)e in this.$?this.$[e]=t[e]:this.$.dataset[e]=t[e]}else this.$=e}cloneNode(t){return new e(this.$.cloneNode(t))}set innerText(e){this.$.innerText=e}get innerText(){return this.$.innerText}get height(){return this.$.offsetHeight}get width(){return this.$.offsetWidth}get id(){return this.$.id}set id(e){this.$.id=e}get value(){return this.$.value}set value(e){this.$.value=e}get src(){return this.$.src}set src(e){this.$.src=e}focus(){this.$.focus()}get classList(){return this.$.classList}get style(){return this.$.style}onchange(e,t,r){return this.$.onchange=n=>{void 0===r?t.apply(e,[n]):r.constructor==Array&&(r.push(n),t.apply(e,r))},this}onclick(e,t,r){return this.$.onclick=n=>{void 0===r?t.apply(e,[n]):r.constructor==Array&&(r.push(n),t.apply(e,r))},this}onup(e,t,r){return this.$.addEventListener("mouseup",(n=>{void 0===r?t.apply(e,[n]):r.constructor==Array&&(r.push(n),t.apply(e,r))})),this}ondown(e,t,r){return this.$.addEventListener("mousedown",(n=>{void 0===r?t.apply(e,[n]):r.constructor==Array&&(r.push(n),t.apply(e,r))})),this}onmove(e,t,r){return this.$.addEventListener("mousemove",(n=>{void 0===r?t.apply(e,[n]):r.constructor==Array&&(r.push(n),t.apply(e,r))})),this}onevent(e,t,r,n){return this.$.addEventListener(e,(e=>{void 0===n?r.apply(t,[e]):n.constructor==Array&&(n.push(e),r.apply(t,n))})),this}append(e){return e.constructor!=Array&&(e=[e]),e.forEach((e=>{/HTML(.*)Element/.test(e.constructor.name)?this.$.appendChild(e):"object"==typeof e&&/HTML(.*)Element/.test(e.$.constructor.name)&&this.$.appendChild(e.$)})),this}delete(){this.$.remove()}removeChilds(){let e=this.$.lastElementChild;for(;e;)this.$.removeChild(e),e=this.$.lastElementChild;return this}static get(t,r){return void 0===(r=r instanceof e?r.$:r)?document.querySelector(t):r.querySelector(t)}static getAll(t,r){return"object"==typeof(r=r instanceof e?r.$:r)?r.querySelectorAll(t):get(r).querySelectorAll(t)}static switchState(t,r){let n=null!=r?r:"on";(t=t instanceof e?t.$:t).classList.contains(n)?t.classList.remove(n):t.classList.add(n)}static UID(){return(+new Date).toString(36)+Math.random().toString(36).substr(2)}static prototypeDetails(t){let r=new e("summary",{innerText:t.innerText}),n=new e("details",{id:t.id,name:t.id}).append(r);return null!=t.onevent&&t.onevent.forEach((e=>{e.args.push(n.$),r.onevent(e.event,e.self,e.fun,e.args)})),n}static prototypeInputFile(t){return new e("label",{htmlFor:`${t.id}_input`,id:t.id,className:t.className,innerText:t.innerText}).append(new e("input",{id:`${t.id}_input`,type:"file"}))}static prototypeCheckSwitch(t){let r=new e("input",{id:t.id,name:t.id,className:"checkswitch",type:"checkbox",value:!1});return[r,new e("div",{className:t.className}).append([new e("div").append([new e("label",{className:"checkswitch",htmlFor:t.id,innerText:t.innerText}).append([r,new e("span")])])])]}static prototypeDownload(e,t){let r,n=/.*\.(py|xml|csv|json|svg|png)$/;if(!n.test(e))return;let a=e.match(n)[1];switch(e=e.replaceAll("/","-").replaceAll(" ","_").toLowerCase(),a){case"xml":r="data:x-application/xml;charset=utf-8,"+encodeURIComponent(t);break;case"py":r="data:text/python;charset=utf-8,"+encodeURIComponent(t);break;case"json":r="data:text/json;charset=utf-8,"+encodeURIComponent(t);break;case"csv":r="data:text/csv;charset=utf-8,"+encodeURIComponent(t);break;case"svg":r="data:image/svg+xml;charset=utf-8,"+encodeURIComponent(t);break;case"png":r=t}let s=document.createElement("a");s.setAttribute("href",r),s.setAttribute("download",e),s.style.display="none",document.body.appendChild(s),s.click(),document.body.removeChild(s)}static setSelected(e,t){for(var r=0;r<e.$.options.length;r++)if(e.$.options[r].text==t)return void(e.$.options[r].selected=!0)}static lazyUpdate(t,r,n,a){a=null==a?"innerText":a;let s=e.get(`[data-uid='${r}']`,t);for(const t in n)e.get(`#${t}`,s)[a]=n[t]}}class t{static randomChar(){const e="abcdefghijklmnopqrstuvwxyz";return e[Math.floor(26*Math.random())]}static UID(){return this.randomChar()+(+new Date).toString(36)+Math.random().toString(36).substring(3)}static SID(){return this.randomChar()+Math.random().toString(36).substring(3,8)}static fromUrl(e){let t=new URLSearchParams(document.location.search);return null==t.get(e)?this.urlDefaults(e):this.urlValidParams(e,t.get(e))}static urlDefaults(e){if("theme"===e)return"light"}static urlValidParams(e,t){if("theme"===e)return["dark","light"].includes(t)?t:this.urlDefaults(e)}static cleanPathname(e){return e.replace(/([^:]\/)\/+/g,"$1").replace(/[^:]\.\//g,"$1")}static getDepth(e){return(e.match(/\//g)||[]).length}static async fetch_each(e){for(let t of e)try{const e=await fetch(t,{method:"Get",headers:{"Content-Type":"application/json"}});if(!0===e.ok)return{obj:await e.json(),url:t};if(404!==e.status)return{error:e,url:t}}catch(e){return{error:e,url:t}}return{error:`No url returned a valid JSON, urls: ${e}`}}static cache_check(e,r,n,a){Array.isArray(r)||(r=[r]);let s=r[0],o=localStorage.getItem(s);null!==o&&(o=JSON.parse(o));let i=new Date(0);i.setHours(n),!0===e.reloaded||null===o||o.timestamp+i.valueOf()<Date.now()?t.fetch_each(r).then((e=>{"error"in e?console.error("Failed to fetch resource, due to:",e.error):(e.timestamp=Date.now(),a(e),localStorage.setItem(s,JSON.stringify(e)))})):a(o)}}class r{constructor(e){this.$={},this.parent=e,this.fetch_tags()}fetch_tags(){t.cache_check(this.parent.state,`/${this.parent.state.repository}/tags.json`,2,(e=>{this.render(e.obj)}))}assert(e){if(Array.isArray(e))return e.every((e=>"string"==typeof e))?"string-array":(console.warn("version_dropdown: expected array of strings, got ",e),!0);if("[object Object]"===Object.prototype.toString.call(e)){for(let t in e){if(!Array.isArray(e[t]))return console.warn("version_dropdown: expected array, got ",e[t]),!0;if(2!==e[t].length)return console.warn(`version_dropdown: expected two items, got ${e[t].length}`,e[t]),!0}return"fine-grained"}return console.warn("version_dropdown: expected object of arrays or array of strings, got ",e),!0}string_array_to_object(e){let t={};e.forEach((e=>{t[e]=[e,""]})),""in t&&(t[""]=["main","unstable"]);for(const e in t){t[e]=[t[e][0],"latest"];break}return t}render(t){let r=this.parent.state.version,n=this.parent.state.path,a="",s=this.assert(t);if(!0===s)return;"string-array"==s&&(t=this.string_array_to_object(t));let o=e.get(".sphinxsidebarwrapper .toc-tree"),i=e.get("header #right .reverse"),c=e.get("body"),l=Object.keys(t).length>10?4:2,p=" auto".repeat(l),h=new e("div",{className:"version-dropdown-list",style:`grid-template-columns:${p}`});t.hasOwnProperty(n)?(a=t[n][1],r=t[n][0]):console.warn(`version_dropdown: current path ${n} is not in the tags.json`,t);for(let r in t){let n=new e("a",{href:`/${this.parent.state.repository}/${r}`}),a=new e("div"),s=new e("div");a.innerText=t[r][0];let o=new e("span",{className:""===t[r][1]?"":"label"});o.innerText=t[r][1],s.append(o),n.append(a),n.append(s),h.append(n)}let d=new e("dev",{id:"cancel-area-show-version-dropdown"});c.append(d.$),c.append(h.$);let u=new e("div",{className:"version-dropdown",title:"Change version"});u.innerText=r;let m=new e("span",{className:""===a?"":"label"});m.innerText=a,u.append(m),o.insertAdjacentElement("afterbegin",u.$);let g=new e(u.$.cloneNode(!0));u.onclick(this,this.show,[!0]),g.onclick(this,this.show,[!0]),d.onclick(this,this.show,[!0]),i.append(g.$),this.$.list=h,this.$.cancel=d}show(t){e.switchState(this.$.cancel),e.switchState(this.$.list)}}class n{constructor(e){this.$={},this.set_doms(),this.parent=e;let t=e.state.metadata;"repotoc"in t&&this.update_repotoc(t.repotoc),"banner"in t&&this.update_banner(t.banner)}set_doms(){let t=this.$;t.repotocTreeOverlay=new e(e.get(".repotoc-tree.overlay root")),t.repotocTreeSidebar=new e(e.get(".sphinxsidebar .repotoc-tree root")),t.banner=new e("div",{className:"banner"}),e.get("body").prepend(t.banner.$)}update_repotoc(t){let r=this.$,n=[],a=[];for(const[r,n]of Object.entries(t)){if(!("name"in n))continue;let t=r==this.parent.state.repository?this.parent.state.content_root:`/${r}/`;a.push(new e("a",{href:`${t}index.html`,className:this.parent.state.repository===r?"current":"",innerText:n.name}))}a.forEach((e=>{n.push(e.cloneNode(!0))})),r.repotocTreeOverlay.$&&(r.repotocTreeOverlay.removeChilds(),r.repotocTreeOverlay.append(n)),r.repotocTreeSidebar.$&&(r.repotocTreeSidebar.removeChilds(),r.repotocTreeSidebar.append(a))}update_banner(t){let r=this.$;"msg"in t&&r.banner.append(new e("span",{innerText:t.msg})),"a_href"in t&&"a_text"in t&&r.banner.append(new e("a",{href:t.a_href,innerText:t.a_text,target:"_blank"}))}}class a{constructor(e){this.$={},this.parent=e,this.page_source()}page_source_sanity(e,t){e.hasOwnProperty("source_hostname")?e.repotoc.hasOwnProperty(t)?e.repotoc[t].hasOwnProperty("pathname")?e.repotoc[t].hasOwnProperty("branch")||console.warn(`edit_source: 'branch' missing from entry '${t}'`):console.warn(`edit_source: 'pathname' missing from entry '${t}'`):console.log(`edit_source: repository '${t}' not in metadata`):console.warn("edit_source: 'source_hostname' missing from metadata",e)}page_source_ignore(){return null!==document.querySelector("#search-documentation")}draw_page_source(t){let r=e.get(".bodywrapper .body"),n=new e("div",{className:"page-actions"}),a=new e("a",{className:"edit-source",title:"See and edit this page source",href:t,target:"blank"});n.append(a),r.insertAdjacentElement("afterbegin",n.$)}page_source(){let e=this.parent.state.metadata,t=this.parent.state.repository;if(this.page_source_sanity(e,t))return;if(this.page_source_ignore())return;let r=e.source_hostname.replace("{repository}",t).replace("{branch}",e.repotoc[t].branch).replace("{pathname}",e.repotoc[t].pathname),n=new URL(location.pathname,location.origin).href,a=new URL(this.parent.state.content_root,n).href,s=n.substring(a.length);s=s.endsWith(".html")?s.replace(/\.html$/i,".rst"):s.concat("index.rst"),r=r.concat("/",s),this.draw_page_source(r)}}function s(){new r(app),new n(app),new a(app)}return s(),s}));
//# sourceMappingURL=extra.umd.js.map
