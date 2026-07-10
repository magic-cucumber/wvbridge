// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
	integrations: [
		starlight({
			title: 'wvbridge',
			description: 'Compose Multiplatform WebView bridge',
			social: [
				{
					icon: 'github',
					label: 'GitHub',
					href: 'https://github.com/magic-cucumber/wvbridge',
				},
			],
			defaultLocale: 'en',
			locales: {
				en: { label: 'English', lang: 'en' },
				zh: { label: '简体中文', lang: 'zh-CN' },
			},
			sidebar: [
				{
					label: 'Documentation',
					translations: { 'zh-CN': '文档' },
					items: [
						{ label: 'Welcome', slug: '', translations: { 'zh-CN': '欢迎' } },
						{ label: 'Installation', slug: 'installation', translations: { 'zh-CN': '安装' } },
						{ label: 'Quick start', link: 'quick-start', translations: { 'zh-CN': '快速开始' } },
						{ label: 'Controller', link: 'controller', translations: { 'zh-CN': 'Controller 导读' } },
						{ label: 'Configuration', link: 'configuration', translations: { 'zh-CN': '配置 WebViewConfig' } },
						{
							label: 'Basic features',
							translations: { 'zh-CN': '基础特性' },
							items: [
								{ label: 'Navigator', link: 'navigator', translations: { 'zh-CN': '导航器' } },
								{ label: 'Interceptor', link: 'interceptor', translations: { 'zh-CN': '拦截器' } },
								{ label: 'JavaScript interop', link: 'javascript-interop', translations: { 'zh-CN': 'JavaScript 互操作性' } },
							],
						},
						{
							label: 'Advanced features',
							translations: { 'zh-CN': '高级特性' },
							items: [
								{ label: 'JavaScript enhancements', link: 'jsbridge-utilities', translations: { 'zh-CN': 'JavaScript 增强' } },
							],
						},
						{ label: 'Known issues', link: 'known-issues', translations: { 'zh-CN': '已知问题' } },
						{
							label: 'API reference',
							link: 'https://wvbridge.kagg886.top/dokka/index.html',
							attrs: { target: '_blank', rel: 'noopener noreferrer' },
							translations: { 'zh-CN': 'API 文档' },
						},
					],
				},
			],
		}),
	],
});
