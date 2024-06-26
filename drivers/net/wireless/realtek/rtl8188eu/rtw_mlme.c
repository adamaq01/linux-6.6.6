// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2016 Realtek Corporation. All rights reserved. */

#define _RTW_MLME_C_

#include <hal_data.h>
#include <usb_osintf.h>

extern void indicate_wx_scan_complete_event(_adapter *padapter);
extern u8 rtw_do_join(_adapter *padapter);


static sint _rtw_init_mlme_priv(_adapter *padapter)
{
	sint	i;
	u8	*pbuf;
	struct wlan_network	*pnetwork;
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	sint	res = _SUCCESS;

	/*ht_priv*/
	pmlmepriv->htpriv.ampdu_enable = false;/*set to disabled*/

	pmlmepriv->nic_hdl = (u8 *)padapter;

	pmlmepriv->pscanned = NULL;
	/*pmlmepriv->fw_state = WIFI_STATION_STATE; */ /*Must sync with rtw_wdev_alloc()*/
	/*init_fwstate(pmlmepriv, WIFI_STATION_STATE);*/
	init_fwstate(pmlmepriv, WIFI_NULL_STATE);/*assigned interface role(STA/AP) must after execute set_opmode*/

	/* wdev->iftype = NL80211_IFTYPE_STATION*/
	pmlmepriv->cur_network.network.InfrastructureMode = Ndis802_11AutoUnknown;
	pmlmepriv->scan_mode = SCAN_ACTIVE; /* 1: active, 0: pasive. Maybe someday we should rename this varable to "active_mode" (Jeff) */

	spin_lock_init(&(pmlmepriv->lock));
	_rtw_init_queue(&(pmlmepriv->free_bss_pool));
	_rtw_init_queue(&(pmlmepriv->scanned_queue));

	set_scanned_network_val(pmlmepriv, 0);

	memset(&pmlmepriv->assoc_ssid, 0, sizeof(NDIS_802_11_SSID));

	pbuf = rtw_zvmalloc(MAX_BSS_CNT * (sizeof(struct wlan_network)));

	if (pbuf == NULL) {
		res = _FAIL;
		goto exit;
	}
	pmlmepriv->free_bss_buf = pbuf;

	pnetwork = (struct wlan_network *)pbuf;

	for (i = 0; i < MAX_BSS_CNT; i++) {
		INIT_LIST_HEAD(&(pnetwork->list));

		list_add_tail(&(pnetwork->list), &(pmlmepriv->free_bss_pool.queue));

		pnetwork++;
	}

	/* allocate DMA-able/Non-Page memory for cmd_buf and rsp_buf */

	rtw_clear_scan_deny(padapter);
#ifdef CONFIG_ARP_KEEP_ALIVE
	pmlmepriv->bGetGateway = 0;
	pmlmepriv->GetGatewayTryCnt = 0;
#endif

#ifdef CONFIG_LAYER2_ROAMING
#define RTW_ROAM_SCAN_RESULT_EXP_MS (5*1000)
#define RTW_ROAM_RSSI_DIFF_TH 10
#define RTW_ROAM_SCAN_INTERVAL_MS (10*1000)
#define RTW_ROAM_RSSI_THRESHOLD 70

	pmlmepriv->roam_flags = 0
				| RTW_ROAM_ON_EXPIRED
#ifdef CONFIG_LAYER2_ROAMING_RESUME
				| RTW_ROAM_ON_RESUME
#endif
#ifdef CONFIG_LAYER2_ROAMING_ACTIVE
				| RTW_ROAM_ACTIVE
#endif
				;

	pmlmepriv->roam_scanr_exp_ms = RTW_ROAM_SCAN_RESULT_EXP_MS;
	pmlmepriv->roam_rssi_diff_th = RTW_ROAM_RSSI_DIFF_TH;
	pmlmepriv->roam_scan_int_ms = RTW_ROAM_SCAN_INTERVAL_MS;
	pmlmepriv->roam_rssi_threshold = RTW_ROAM_RSSI_THRESHOLD;
#endif /* CONFIG_LAYER2_ROAMING */

#ifdef CONFIG_RTW_80211R
	memset(&pmlmepriv->ftpriv, 0, sizeof(ft_priv));
	pmlmepriv->ftpriv.ft_flags = 0
		| RTW_FT_STA_SUPPORTED
		| RTW_FT_STA_OVER_DS_SUPPORTED
		;
#endif
	rtw_init_mlme_timer(padapter);

exit:


	return res;
}

static void rtw_mfree_mlme_priv_lock(struct mlme_priv *pmlmepriv)
{
}

static void rtw_free_mlme_ie_data(u8 **ppie, u32 *plen)
{
	if (*ppie) {
		rtw_mfree(*ppie, *plen);
		*plen = 0;
		*ppie = NULL;
	}
}

void rtw_free_mlme_priv_ie_data(struct mlme_priv *pmlmepriv)
{
#if defined(CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME)
	rtw_buf_free(&pmlmepriv->assoc_req, &pmlmepriv->assoc_req_len);
	rtw_buf_free(&pmlmepriv->assoc_rsp, &pmlmepriv->assoc_rsp_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_beacon_ie, &pmlmepriv->wps_beacon_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_probe_req_ie, &pmlmepriv->wps_probe_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_probe_resp_ie, &pmlmepriv->wps_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_assoc_resp_ie, &pmlmepriv->wps_assoc_resp_ie_len);

	rtw_free_mlme_ie_data(&pmlmepriv->p2p_beacon_ie, &pmlmepriv->p2p_beacon_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_probe_req_ie, &pmlmepriv->p2p_probe_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_probe_resp_ie, &pmlmepriv->p2p_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_go_probe_resp_ie, &pmlmepriv->p2p_go_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_assoc_req_ie, &pmlmepriv->p2p_assoc_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_assoc_resp_ie, &pmlmepriv->p2p_assoc_resp_ie_len);
#endif

#if defined(CONFIG_WFD) && defined(CONFIG_IOCTL_CFG80211)
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_beacon_ie, &pmlmepriv->wfd_beacon_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_probe_req_ie, &pmlmepriv->wfd_probe_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_probe_resp_ie, &pmlmepriv->wfd_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_go_probe_resp_ie, &pmlmepriv->wfd_go_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_assoc_req_ie, &pmlmepriv->wfd_assoc_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_assoc_resp_ie, &pmlmepriv->wfd_assoc_resp_ie_len);
#endif

#ifdef CONFIG_RTW_80211R
	rtw_free_mlme_ie_data(&pmlmepriv->auth_rsp, &pmlmepriv->auth_rsp_len);
#endif
}

#if defined(CONFIG_WFD) && defined(CONFIG_IOCTL_CFG80211)
int rtw_mlme_update_wfd_ie_data(struct mlme_priv *mlme, u8 type, u8 *ie, u32 ie_len)
{
	_adapter *adapter = mlme_to_adapter(mlme);
	struct wifi_display_info *wfd_info = &adapter->wfd_info;
	u8 clear = 0;
	u8 **t_ie = NULL;
	u32 *t_ie_len = NULL;
	int ret = _FAIL;

	if (!hal_chk_wl_func(adapter, WL_FUNC_MIRACAST))
		goto success;

	if (wfd_info->wfd_enable)
		goto success; /* WFD IE is build by self */

	if (!ie && !ie_len)
		clear = 1;
	else if (!ie || !ie_len) {
		RTW_PRINT(FUNC_ADPT_FMT" type:%u, ie:%p, ie_len:%u"
			  , FUNC_ADPT_ARG(adapter), type, ie, ie_len);
		rtw_warn_on(1);
		goto exit;
	}

	switch (type) {
	case MLME_BEACON_IE:
		t_ie = &mlme->wfd_beacon_ie;
		t_ie_len = &mlme->wfd_beacon_ie_len;
		break;
	case MLME_PROBE_REQ_IE:
		t_ie = &mlme->wfd_probe_req_ie;
		t_ie_len = &mlme->wfd_probe_req_ie_len;
		break;
	case MLME_PROBE_RESP_IE:
		t_ie = &mlme->wfd_probe_resp_ie;
		t_ie_len = &mlme->wfd_probe_resp_ie_len;
		break;
	case MLME_GO_PROBE_RESP_IE:
		t_ie = &mlme->wfd_go_probe_resp_ie;
		t_ie_len = &mlme->wfd_go_probe_resp_ie_len;
		break;
	case MLME_ASSOC_REQ_IE:
		t_ie = &mlme->wfd_assoc_req_ie;
		t_ie_len = &mlme->wfd_assoc_req_ie_len;
		break;
	case MLME_ASSOC_RESP_IE:
		t_ie = &mlme->wfd_assoc_resp_ie;
		t_ie_len = &mlme->wfd_assoc_resp_ie_len;
		break;
	default:
		RTW_PRINT(FUNC_ADPT_FMT" unsupported type:%u"
			  , FUNC_ADPT_ARG(adapter), type);
		rtw_warn_on(1);
		goto exit;
	}

	if (*t_ie) {
		u32 free_len = *t_ie_len;
		*t_ie_len = 0;
		rtw_mfree(*t_ie, free_len);
		*t_ie = NULL;
	}

	if (!clear) {
		*t_ie = rtw_malloc(ie_len);
		if (*t_ie == NULL) {
			RTW_ERR(FUNC_ADPT_FMT" type:%u, rtw_malloc() fail\n"
				, FUNC_ADPT_ARG(adapter), type);
			goto exit;
		}
		memcpy(*t_ie, ie, ie_len);
		*t_ie_len = ie_len;
	}

	if (*t_ie && *t_ie_len) {
		u8 *attr_content;
		u32 attr_contentlen = 0;

		attr_content = rtw_get_wfd_attr_content(*t_ie, *t_ie_len, WFD_ATTR_DEVICE_INFO, NULL, &attr_contentlen);
		if (attr_content && attr_contentlen) {
			if (RTW_GET_BE16(attr_content + 2) != wfd_info->rtsp_ctrlport) {
				wfd_info->rtsp_ctrlport = RTW_GET_BE16(attr_content + 2);
				RTW_INFO(FUNC_ADPT_FMT" type:%u, RTSP CTRL port = %u\n"
					, FUNC_ADPT_ARG(adapter), type, wfd_info->rtsp_ctrlport);
			}
		}
	}

success:
	ret = _SUCCESS;

exit:
	return ret;
}
#endif /* defined(CONFIG_WFD) && defined(CONFIG_IOCTL_CFG80211) */

static void _rtw_free_mlme_priv(struct mlme_priv *pmlmepriv)
{
	if (NULL == pmlmepriv) {
		rtw_warn_on(1);
		goto exit;
	}
	rtw_free_mlme_priv_ie_data(pmlmepriv);

	if (pmlmepriv) {
		rtw_mfree_mlme_priv_lock(pmlmepriv);

		if (pmlmepriv->free_bss_buf)
			rtw_vmfree(pmlmepriv->free_bss_buf, MAX_BSS_CNT * sizeof(struct wlan_network));
	}
exit:
	return;
}

static sint	_rtw_enqueue_network(_queue *queue, struct wlan_network *pnetwork)
{
	unsigned long irqL;


	if (pnetwork == NULL)
		goto exit;

	_enter_critical_bh(&queue->lock, &irqL);

	list_add_tail(&pnetwork->list, &queue->queue);

	_exit_critical_bh(&queue->lock, &irqL);

exit:


	return _SUCCESS;
}

/*
struct	wlan_network *_rtw_dequeue_network(_queue *queue)
{
	unsigned long irqL;

	struct wlan_network *pnetwork;


	_enter_critical_bh(&queue->lock, &irqL);

	if (_rtw_queue_empty(queue))

		pnetwork = NULL;

	else
	{
		pnetwork = LIST_CONTAINOR(get_next(&queue->queue), struct wlan_network, list);

		list_del_init(&(pnetwork->list));
	}

	_exit_critical_bh(&queue->lock, &irqL);


	return pnetwork;
}
*/

struct	wlan_network *_rtw_alloc_network(struct	mlme_priv *pmlmepriv) /* (_queue *free_queue) */
{
	unsigned long	irqL;
	struct	wlan_network	*pnetwork;
	_queue *free_queue = &pmlmepriv->free_bss_pool;
	_list *plist = NULL;


	_enter_critical_bh(&free_queue->lock, &irqL);

	if (_rtw_queue_empty(free_queue)) {
		pnetwork = NULL;
		goto exit;
	}
	plist = get_next(&(free_queue->queue));

	pnetwork = LIST_CONTAINOR(plist , struct wlan_network, list);

	list_del_init(&pnetwork->list);

	pnetwork->network_type = 0;
	pnetwork->fixed = false;
	pnetwork->last_scanned = jiffies;
	pnetwork->aid = 0;
	pnetwork->join_res = 0;

	pmlmepriv->num_of_scanned++;

exit:
	_exit_critical_bh(&free_queue->lock, &irqL);


	return pnetwork;
}

void _rtw_free_network(struct	mlme_priv *pmlmepriv , struct wlan_network *pnetwork, u8 isfreeall)
{
	u32 delta_time;
	u32 lifetime = SCANQUEUE_LIFETIME;
	unsigned long irqL;
	_queue *free_queue = &(pmlmepriv->free_bss_pool);


	if (pnetwork == NULL)
		goto exit;

	if (pnetwork->fixed)
		goto exit;

	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)))
		lifetime = 1;

	if (!isfreeall) {
		delta_time = (u32) rtw_get_passing_time_ms(pnetwork->last_scanned);
		if (delta_time < lifetime) /* unit:msec */
			goto exit;
	}

	_enter_critical_bh(&free_queue->lock, &irqL);

	list_del_init(&(pnetwork->list));

	list_add_tail(&(pnetwork->list), &(free_queue->queue));

	pmlmepriv->num_of_scanned--;


	/* RTW_INFO("_rtw_free_network:SSID=%s\n", pnetwork->network.Ssid.Ssid); */

	_exit_critical_bh(&free_queue->lock, &irqL);

exit:
	return;
}

void _rtw_free_network_nolock(struct	mlme_priv *pmlmepriv, struct wlan_network *pnetwork)
{

	_queue *free_queue = &(pmlmepriv->free_bss_pool);


	if (pnetwork == NULL)
		goto exit;

	if (pnetwork->fixed)
		goto exit;

	/* _enter_critical(&free_queue->lock, &irqL); */

	list_del_init(&(pnetwork->list));

	list_add_tail(&(pnetwork->list), get_list_head(free_queue));

	pmlmepriv->num_of_scanned--;

	/* _exit_critical(&free_queue->lock, &irqL); */

exit:
	return;
}


/*
	return the wlan_network with the matching addr

	Shall be calle under atomic context... to avoid possible racing condition...
*/
struct wlan_network *_rtw_find_network(_queue *scanned_queue, u8 *addr)
{

	/* unsigned long irqL; */
	_list	*phead, *plist;
	struct	wlan_network *pnetwork = NULL;
	u8 zero_addr[ETH_ALEN] = {0, 0, 0, 0, 0, 0};


	if (!memcmp(zero_addr, addr, ETH_ALEN)) {
		pnetwork = NULL;
		goto exit;
	}

	/* _enter_critical_bh(&scanned_queue->lock, &irqL); */

	phead = get_list_head(scanned_queue);
	plist = get_next(phead);

	while (plist != phead) {
		pnetwork = LIST_CONTAINOR(plist, struct wlan_network , list);

		if (!memcmp(addr, pnetwork->network.MacAddress, ETH_ALEN))
			break;

		plist = get_next(plist);
	}

	if (plist == phead)
		pnetwork = NULL;

	/* _exit_critical_bh(&scanned_queue->lock, &irqL); */

exit:


	return pnetwork;

}


void _rtw_free_network_queue(_adapter *padapter, u8 isfreeall)
{
	unsigned long irqL;
	_list *phead, *plist;
	struct wlan_network *pnetwork;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *scanned_queue = &pmlmepriv->scanned_queue;



	_enter_critical_bh(&scanned_queue->lock, &irqL);

	phead = get_list_head(scanned_queue);
	plist = get_next(phead);

	while (rtw_end_of_queue_search(phead, plist) == false) {

		pnetwork = LIST_CONTAINOR(plist, struct wlan_network, list);

		plist = get_next(plist);

		_rtw_free_network(pmlmepriv, pnetwork, isfreeall);

	}

	_exit_critical_bh(&scanned_queue->lock, &irqL);


}




sint rtw_if_up(_adapter *padapter)
{

	sint res;

	if (RTW_CANNOT_RUN(padapter) ||
	    (check_fwstate(&padapter->mlmepriv, _FW_LINKED) == false)) {
		res = false;
	} else
		res =  true;

	return res;
}


void rtw_generate_random_ibss(u8 *pibss)
{
	*((u32 *)(&pibss[2])) = rtw_random32();
	pibss[0] = 0x02; /* in ad-hoc mode local bit must set to 1 */
	pibss[1] = 0x11;
	pibss[2] = 0x87;
}

u8 *rtw_get_capability_from_ie(u8 *ie)
{
	return ie + 8 + 2;
}


u16 rtw_get_capability(WLAN_BSSID_EX *bss)
{
	__le16	val;

	memcpy((u8 *)&val, rtw_get_capability_from_ie(bss->IEs), 2);
	return le16_to_cpu(val);
}

u8 *rtw_get_timestampe_from_ie(u8 *ie)
{
	return ie + 0;
}

u8 *rtw_get_beacon_interval_from_ie(u8 *ie)
{
	return ie + 8;
}


int	rtw_init_mlme_priv(_adapter *padapter) /* (struct	mlme_priv *pmlmepriv) */
{
	int	res;
	res = _rtw_init_mlme_priv(padapter);/* (pmlmepriv); */
	return res;
}

void rtw_free_mlme_priv(struct mlme_priv *pmlmepriv)
{
	_rtw_free_mlme_priv(pmlmepriv);
}

int	rtw_enqueue_network(_queue *queue, struct wlan_network *pnetwork);
int	rtw_enqueue_network(_queue *queue, struct wlan_network *pnetwork)
{
	int	res;
	res = _rtw_enqueue_network(queue, pnetwork);
	return res;
}

/*
static struct	wlan_network *rtw_dequeue_network(_queue *queue)
{
	struct wlan_network *pnetwork;
	pnetwork = _rtw_dequeue_network(queue);
	return pnetwork;
}
*/

struct	wlan_network *rtw_alloc_network(struct	mlme_priv *pmlmepriv);
struct	wlan_network *rtw_alloc_network(struct	mlme_priv *pmlmepriv) /* (_queue	*free_queue) */
{
	struct	wlan_network	*pnetwork;
	pnetwork = _rtw_alloc_network(pmlmepriv);
	return pnetwork;
}

void rtw_free_network(struct mlme_priv *pmlmepriv, struct	wlan_network *pnetwork, u8 is_freeall);
void rtw_free_network(struct mlme_priv *pmlmepriv, struct	wlan_network *pnetwork, u8 is_freeall)/* (struct	wlan_network *pnetwork, _queue	*free_queue) */
{
	_rtw_free_network(pmlmepriv, pnetwork, is_freeall);
}

void rtw_free_network_nolock(_adapter *padapter, struct wlan_network *pnetwork);
void rtw_free_network_nolock(_adapter *padapter, struct wlan_network *pnetwork)
{
	_rtw_free_network_nolock(&(padapter->mlmepriv), pnetwork);
#ifdef CONFIG_IOCTL_CFG80211
	rtw_cfg80211_unlink_bss(padapter, pnetwork);
#endif /* CONFIG_IOCTL_CFG80211 */
}


void rtw_free_network_queue(_adapter *dev, u8 isfreeall)
{
	_rtw_free_network_queue(dev, isfreeall);
}

/*
	return the wlan_network with the matching addr

	Shall be calle under atomic context... to avoid possible racing condition...
*/
struct	wlan_network *rtw_find_network(_queue *scanned_queue, u8 *addr)
{
	struct	wlan_network *pnetwork = _rtw_find_network(scanned_queue, addr);

	return pnetwork;
}

int rtw_is_same_ibss(_adapter *adapter, struct wlan_network *pnetwork)
{
	int ret = true;
	struct security_priv *psecuritypriv = &adapter->securitypriv;

	if ((psecuritypriv->dot11PrivacyAlgrthm != _NO_PRIVACY_) &&
	    (pnetwork->network.Privacy == 0))
		ret = false;
	else if ((psecuritypriv->dot11PrivacyAlgrthm == _NO_PRIVACY_) &&
		 (pnetwork->network.Privacy == 1))
		ret = false;
	else
		ret = true;

	return ret;

}

inline int is_same_ess(WLAN_BSSID_EX *a, WLAN_BSSID_EX *b)
{
	return (a->Ssid.SsidLength == b->Ssid.SsidLength)
	       &&  !memcmp(a->Ssid.Ssid, b->Ssid.Ssid, a->Ssid.SsidLength);
}

int is_same_network(WLAN_BSSID_EX *src, WLAN_BSSID_EX *dst, u8 feature)
{
	u16 s_cap, d_cap;
	__le16 le_tmp;

	if (rtw_bug_check(dst, src, &s_cap, &d_cap) == false)
		return false;

	memcpy((u8 *)&le_tmp, rtw_get_capability_from_ie(src->IEs), 2);
	s_cap = le16_to_cpu(le_tmp);
	memcpy((u8 *)&le_tmp, rtw_get_capability_from_ie(dst->IEs), 2);
	d_cap = le16_to_cpu(le_tmp);

#ifdef CONFIG_P2P
	if ((feature == 1) && /* 1: P2P supported */
	    (!memcmp(src->MacAddress, dst->MacAddress, ETH_ALEN))
	   )
		return true;
#endif

	return ((src->Ssid.SsidLength == dst->Ssid.SsidLength) &&
		/*	(src->Configuration.DSConfig == dst->Configuration.DSConfig) && */
		((!memcmp(src->MacAddress, dst->MacAddress, ETH_ALEN))) &&
		((!memcmp(src->Ssid.Ssid, dst->Ssid.Ssid, src->Ssid.SsidLength))) &&
		((s_cap & WLAN_CAPABILITY_IBSS) ==
		 (d_cap & WLAN_CAPABILITY_IBSS)) &&
		((s_cap & WLAN_CAPABILITY_BSS) ==
		 (d_cap & WLAN_CAPABILITY_BSS)));
}

struct wlan_network *_rtw_find_same_network(_queue *scanned_queue, struct wlan_network *network)
{
	_list *phead, *plist;
	struct wlan_network *found = NULL;

	phead = get_list_head(scanned_queue);
	plist = get_next(phead);

	while (plist != phead) {
		found = LIST_CONTAINOR(plist, struct wlan_network , list);

		if (is_same_network(&network->network, &found->network, 0))
			break;

		plist = get_next(plist);
	}

	if (plist == phead)
		found = NULL;
exit:
	return found;
}

struct wlan_network *rtw_find_same_network(_queue *scanned_queue, struct wlan_network *network)
{
	unsigned long irqL;
	struct wlan_network *found = NULL;

	if (scanned_queue == NULL || network == NULL)
		goto exit;

	_enter_critical_bh(&scanned_queue->lock, &irqL);
	found = _rtw_find_same_network(scanned_queue, network);
	_exit_critical_bh(&scanned_queue->lock, &irqL);

exit:
	return found;
}

struct	wlan_network	*rtw_get_oldest_wlan_network(_queue *scanned_queue)
{
	_list	*plist, *phead;


	struct	wlan_network	*pwlan = NULL;
	struct	wlan_network	*oldest = NULL;
	phead = get_list_head(scanned_queue);

	plist = get_next(phead);

	while (1) {

		if (rtw_end_of_queue_search(phead, plist))
			break;

		pwlan = LIST_CONTAINOR(plist, struct wlan_network, list);

		if (pwlan->fixed != true) {
			if (oldest == NULL || time_after(oldest->last_scanned, pwlan->last_scanned))
				oldest = pwlan;
		}

		plist = get_next(plist);
	}
	return oldest;

}

void update_network(WLAN_BSSID_EX *dst, WLAN_BSSID_EX *src,
		    _adapter *padapter, bool update_ie)
{
	u8 ss_ori = dst->PhyInfo.SignalStrength;
	u8 sq_ori = dst->PhyInfo.SignalQuality;
	long rssi_ori = dst->Rssi;

	u8 ss_smp = src->PhyInfo.SignalStrength;
	u8 sq_smp = src->PhyInfo.SignalQuality;
	long rssi_smp = src->Rssi;

	u8 ss_final;
	u8 sq_final;
	long rssi_final;


#ifdef CONFIG_ANTENNA_DIVERSITY
	rtw_hal_antdiv_rssi_compared(padapter, dst, src); /* this will update src.Rssi, need consider again */
#endif

#if defined(DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) && 1
	if (strcmp(dst->Ssid.Ssid, DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) == 0) {
		RTW_INFO(FUNC_ADPT_FMT" %s("MAC_FMT", ch%u) ss_ori:%3u, sq_ori:%3u, rssi_ori:%3ld, ss_smp:%3u, sq_smp:%3u, rssi_smp:%3ld\n"
			 , FUNC_ADPT_ARG(padapter)
			, src->Ssid.Ssid, MAC_ARG(src->MacAddress), src->Configuration.DSConfig
			 , ss_ori, sq_ori, rssi_ori
			 , ss_smp, sq_smp, rssi_smp
			);
	}
#endif

	/* The rule below is 1/5 for sample value, 4/5 for history value */
	if (check_fwstate(&padapter->mlmepriv, _FW_LINKED) && is_same_network(&(padapter->mlmepriv.cur_network.network), src, 0)) {
		/* Take the recvpriv's value for the connected AP*/
		ss_final = padapter->recvpriv.signal_strength;
		sq_final = padapter->recvpriv.signal_qual;
		/* the rssi value here is undecorated, and will be used for antenna diversity */
		if (sq_smp != 101) /* from the right channel */
			rssi_final = (src->Rssi + dst->Rssi * 4) / 5;
		else
			rssi_final = rssi_ori;
	} else {
		if (sq_smp != 101) { /* from the right channel */
			ss_final = ((u32)(src->PhyInfo.SignalStrength) + (u32)(dst->PhyInfo.SignalStrength) * 4) / 5;
			sq_final = ((u32)(src->PhyInfo.SignalQuality) + (u32)(dst->PhyInfo.SignalQuality) * 4) / 5;
			rssi_final = (src->Rssi + dst->Rssi * 4) / 5;
		} else {
			/* bss info not receving from the right channel, use the original RX signal infos */
			ss_final = dst->PhyInfo.SignalStrength;
			sq_final = dst->PhyInfo.SignalQuality;
			rssi_final = dst->Rssi;
		}

	}

	if (update_ie) {
		dst->Reserved[0] = src->Reserved[0];
		dst->Reserved[1] = src->Reserved[1];
		memcpy((u8 *)dst, (u8 *)src, get_WLAN_BSSID_EX_sz(src));
	}

	dst->PhyInfo.SignalStrength = ss_final;
	dst->PhyInfo.SignalQuality = sq_final;
	dst->Rssi = rssi_final;

#if defined(DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) && 1
	if (strcmp(dst->Ssid.Ssid, DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) == 0) {
		RTW_INFO(FUNC_ADPT_FMT" %s("MAC_FMT"), SignalStrength:%u, SignalQuality:%u, RawRSSI:%ld\n"
			 , FUNC_ADPT_ARG(padapter)
			, dst->Ssid.Ssid, MAC_ARG(dst->MacAddress), dst->PhyInfo.SignalStrength, dst->PhyInfo.SignalQuality, dst->Rssi);
	}
#endif
}

static void update_current_network(_adapter *adapter, WLAN_BSSID_EX *pnetwork)
{
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);


	rtw_bug_check(&(pmlmepriv->cur_network.network),
		      &(pmlmepriv->cur_network.network),
		      &(pmlmepriv->cur_network.network),
		      &(pmlmepriv->cur_network.network));

	if ((check_fwstate(pmlmepriv, _FW_LINKED)) && (is_same_network(&(pmlmepriv->cur_network.network), pnetwork, 0))) {
		update_network(&(pmlmepriv->cur_network.network), pnetwork, adapter, true);
		rtw_update_protection(adapter, (pmlmepriv->cur_network.network.IEs) + sizeof(NDIS_802_11_FIXED_IEs),
				      pmlmepriv->cur_network.network.IELength);
	}


}


/*

Caller must hold pmlmepriv->lock first.


*/
void rtw_update_scanned_network(_adapter *adapter, WLAN_BSSID_EX *target)
{
	unsigned long irqL;
	_list	*plist, *phead;
	u32	bssid_ex_sz;
	struct mlme_priv	*pmlmepriv = &(adapter->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(adapter->mlmeextpriv);
#ifdef CONFIG_P2P
	struct wifidirect_info *pwdinfo = &(adapter->wdinfo);
#endif /* CONFIG_P2P */
	_queue	*queue	= &(pmlmepriv->scanned_queue);
	struct wlan_network	*pnetwork = NULL;
	struct wlan_network	*oldest = NULL;
	int target_find = 0;
	u8 feature = 0;


	_enter_critical_bh(&queue->lock, &irqL);
	phead = get_list_head(queue);
	plist = get_next(phead);

#ifdef CONFIG_P2P
	if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
		feature = 1; /* p2p enable */
#endif

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = LIST_CONTAINOR(plist, struct wlan_network, list);

		rtw_bug_check(pnetwork, pnetwork, pnetwork, pnetwork);

#ifdef CONFIG_P2P
		if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE) &&
		    (!memcmp(pnetwork->network.MacAddress, target->MacAddress, ETH_ALEN))) {
			target_find = 1;
			break;
		}
#endif

		if (is_same_network(&(pnetwork->network), target, feature)) {
			target_find = 1;
			break;
		}

		if (rtw_roam_flags(adapter)) {
			/* TODO: don't  select netowrk in the same ess as oldest if it's new enough*/
		}

		if (oldest == NULL || time_after(oldest->last_scanned, pnetwork->last_scanned))
			oldest = pnetwork;

		plist = get_next(plist);

	}


	/* If we didn't find a match, then get a new network slot to initialize
	 * with this beacon's information */
	/* if (rtw_end_of_queue_search(phead,plist)== true) { */
	if (!target_find) {
		if (_rtw_queue_empty(&(pmlmepriv->free_bss_pool))) {
			/* If there are no more slots, expire the oldest */
			/* list_del_init(&oldest->list); */
			pnetwork = oldest;
			if (pnetwork == NULL) {
				goto exit;
			}
#ifdef CONFIG_ANTENNA_DIVERSITY
			rtw_hal_get_odm_var(adapter, HAL_ODM_ANTDIV_SELECT, &(target->PhyInfo.Optimum_antenna), NULL);
#endif
			memcpy(&(pnetwork->network), target,  get_WLAN_BSSID_EX_sz(target));
			/* pnetwork->last_scanned = jiffies; */
			/* variable initialize */
			pnetwork->fixed = false;
			pnetwork->last_scanned = jiffies;

			pnetwork->network_type = 0;
			pnetwork->aid = 0;
			pnetwork->join_res = 0;

			/* bss info not receving from the right channel */
			if (pnetwork->network.PhyInfo.SignalQuality == 101)
				pnetwork->network.PhyInfo.SignalQuality = 0;
		} else {
			/* Otherwise just pull from the free list */

			pnetwork = rtw_alloc_network(pmlmepriv); /* will update scan_time */

			if (pnetwork == NULL) {
				goto exit;
			}

			bssid_ex_sz = get_WLAN_BSSID_EX_sz(target);
			target->Length = bssid_ex_sz;
#ifdef CONFIG_ANTENNA_DIVERSITY
			rtw_hal_get_odm_var(adapter, HAL_ODM_ANTDIV_SELECT, &(target->PhyInfo.Optimum_antenna), NULL);
#endif
			memcpy(&(pnetwork->network), target, bssid_ex_sz);

			pnetwork->last_scanned = jiffies;

			/* bss info not receving from the right channel */
			if (pnetwork->network.PhyInfo.SignalQuality == 101)
				pnetwork->network.PhyInfo.SignalQuality = 0;

			list_add_tail(&(pnetwork->list), &(queue->queue));

		}
	} else {
		/* we have an entry and we are going to update it. But this entry may
		 * be already expired. In this case we do the same as we found a new
		 * net and call the new_net handler
		 */
		bool update_ie = true;

		pnetwork->last_scanned = jiffies;

		/* target.Reserved[0]==1, means that scaned network is a bcn frame. */
		if ((pnetwork->network.IELength > target->IELength) && (target->Reserved[0] == 1))
			update_ie = false;

		/* probe resp(3) > beacon(1) > probe req(2) */
		if ((target->Reserved[0] != 2) &&
		    (target->Reserved[0] >= pnetwork->network.Reserved[0])
		   )
			update_ie = true;
		else
			update_ie = false;

		update_network(&(pnetwork->network), target, adapter, update_ie);
	}

exit:
	_exit_critical_bh(&queue->lock, &irqL);

}

void rtw_add_network(_adapter *adapter, WLAN_BSSID_EX *pnetwork);
void rtw_add_network(_adapter *adapter, WLAN_BSSID_EX *pnetwork)
{
	unsigned long irqL;
	struct	mlme_priv	*pmlmepriv = &(((_adapter *)adapter)->mlmepriv);
	/* _queue	*queue	= &(pmlmepriv->scanned_queue); */


	/* _enter_critical_bh(&queue->lock, &irqL); */

#if defined(CONFIG_P2P) && defined(CONFIG_P2P_REMOVE_GROUP_INFO)
	if (adapter->registrypriv.wifi_spec == 0)
		rtw_bss_ex_del_p2p_attr(pnetwork, P2P_ATTR_GROUP_INFO);
#endif

	if (!hal_chk_wl_func(adapter, WL_FUNC_MIRACAST))
		rtw_bss_ex_del_wfd_ie(pnetwork);

	update_current_network(adapter, pnetwork);

	rtw_update_scanned_network(adapter, pnetwork);

	/* _exit_critical_bh(&queue->lock, &irqL); */

}

/* select the desired network based on the capability of the (i)bss.
 * check items: (1) security
 *			   (2) network_type
 *			   (3) WMM
 *			   (4) HT
 * (5) others */
int rtw_is_desired_network(_adapter *adapter, struct wlan_network *pnetwork);
int rtw_is_desired_network(_adapter *adapter, struct wlan_network *pnetwork)
{
	struct security_priv *psecuritypriv = &adapter->securitypriv;
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	u32 desired_encmode;
	u32 privacy;

	/* u8 wps_ie[512]; */
	uint wps_ielen;

	int bselected = true;

	desired_encmode = psecuritypriv->ndisencryptstatus;
	privacy = pnetwork->network.Privacy;

	if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		if (rtw_get_wps_ie(pnetwork->network.IEs + _FIXED_IE_LENGTH_, pnetwork->network.IELength - _FIXED_IE_LENGTH_, NULL, &wps_ielen) != NULL)
			return true;
		else
			return false;
	}
	if (adapter->registrypriv.wifi_spec == 1) { /* for  correct flow of 8021X  to do.... */
		u8 *p = NULL;
		uint ie_len = 0;

		if ((desired_encmode == Ndis802_11EncryptionDisabled) && (privacy != 0))
			bselected = false;

		if (psecuritypriv->ndisauthtype == Ndis802_11AuthModeWPA2PSK) {
			p = rtw_get_ie(pnetwork->network.IEs + _BEACON_IE_OFFSET_, _RSN_IE_2_, &ie_len, (pnetwork->network.IELength - _BEACON_IE_OFFSET_));
			if (p && ie_len > 0)
				bselected = true;
			else
				bselected = false;
		}
	}


	if ((desired_encmode != Ndis802_11EncryptionDisabled) && (privacy == 0)) {
		RTW_INFO("desired_encmode: %d, privacy: %d\n", desired_encmode, privacy);
		bselected = false;
	}

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) {
		if (pnetwork->network.InfrastructureMode != pmlmepriv->cur_network.network.InfrastructureMode)
			bselected = false;
	}


	return bselected;
}

/* TODO: Perry : For Power Management */
void rtw_atimdone_event_callback(_adapter	*adapter , u8 *pbuf)
{

	return;
}


void rtw_survey_event_callback(_adapter	*adapter, u8 *pbuf)
{
	unsigned long  irqL;
	u32 len;
	WLAN_BSSID_EX *pnetwork;
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);


	pnetwork = (WLAN_BSSID_EX *)pbuf;

	len = get_WLAN_BSSID_EX_sz(pnetwork);
	if (len > (sizeof(WLAN_BSSID_EX))) {
		return;
	}


	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	/* update IBSS_network 's timestamp */
	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) {
		if (!memcmp(&(pmlmepriv->cur_network.network.MacAddress), pnetwork->MacAddress, ETH_ALEN)) {
			struct wlan_network *ibss_wlan = NULL;
			unsigned long	irqL;

			memcpy(pmlmepriv->cur_network.network.IEs, pnetwork->IEs, 8);
			_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			ibss_wlan = rtw_find_network(&pmlmepriv->scanned_queue,  pnetwork->MacAddress);
			if (ibss_wlan) {
				memcpy(ibss_wlan->network.IEs , pnetwork->IEs, 8);
				_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
				goto exit;
			}
			_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		}
	}

	/* lock pmlmepriv->lock when you accessing network_q */
	if ((check_fwstate(pmlmepriv, _FW_UNDER_LINKING)) == false) {
		if (pnetwork->Ssid.Ssid[0] == 0)
			pnetwork->Ssid.SsidLength = 0;
		rtw_add_network(adapter, pnetwork);
	}

exit:

	_exit_critical_bh(&pmlmepriv->lock, &irqL);


	return;
}

void rtw_surveydone_event_callback(_adapter	*adapter, u8 *pbuf)
{
	unsigned long  irqL;
	u8 timer_cancelled;
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);
#ifdef CONFIG_RTW_80211R
	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;
#endif

#ifdef CONFIG_MLME_EXT
	mlmeext_surveydone_event_callback(adapter);
#endif


	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	if (pmlmepriv->wps_probe_req_ie) {
		u32 free_len = pmlmepriv->wps_probe_req_ie_len;
		pmlmepriv->wps_probe_req_ie_len = 0;
		rtw_mfree(pmlmepriv->wps_probe_req_ie, free_len);
		pmlmepriv->wps_probe_req_ie = NULL;
	}


	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY) == false) {
		RTW_INFO(FUNC_ADPT_FMT" fw_state:0x%x\n", FUNC_ADPT_ARG(adapter), get_fwstate(pmlmepriv));
		/* rtw_warn_on(1); */
	}

	_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY);
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

	_cancel_timer(&pmlmepriv->scan_to_timer, &timer_cancelled);

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

#ifdef CONFIG_NEW_SIGNAL_STAT_PROCESS
	rtw_set_signal_stat_timer(&adapter->recvpriv);
#endif

	if (pmlmepriv->to_join) {
		if ((check_fwstate(pmlmepriv, WIFI_ADHOC_STATE))) {
			if (check_fwstate(pmlmepriv, _FW_LINKED) == false) {
				set_fwstate(pmlmepriv, _FW_UNDER_LINKING);

				if (rtw_select_and_join_from_scanned_queue(pmlmepriv) == _SUCCESS)
					_set_timer(&pmlmepriv->assoc_timer, MAX_JOIN_TIMEOUT);
				else {
					WLAN_BSSID_EX    *pdev_network = &(adapter->registrypriv.dev_network);
					u8 *pibss = adapter->registrypriv.dev_network.MacAddress;

					/* pmlmepriv->fw_state ^= _FW_UNDER_SURVEY; */ /* because don't set assoc_timer */
					_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY);


					memset(&pdev_network->Ssid, 0, sizeof(NDIS_802_11_SSID));
					memcpy(&pdev_network->Ssid, &pmlmepriv->assoc_ssid, sizeof(NDIS_802_11_SSID));

					rtw_update_registrypriv_dev_network(adapter);
					rtw_generate_random_ibss(pibss);

					/*pmlmepriv->fw_state = WIFI_ADHOC_MASTER_STATE;*/
//					init_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE);

					if (rtw_create_ibss_cmd(adapter, 0) != _SUCCESS)
						RTW_ERR("rtw_create_ibss_cmd FAIL\n");

					pmlmepriv->to_join = false;
				}
			}
		} else {
			int s_ret;
			set_fwstate(pmlmepriv, _FW_UNDER_LINKING);
			pmlmepriv->to_join = false;
			s_ret = rtw_select_and_join_from_scanned_queue(pmlmepriv);
			if (_SUCCESS == s_ret)
				_set_timer(&pmlmepriv->assoc_timer, MAX_JOIN_TIMEOUT);
			else if (s_ret == 2) { /* there is no need to wait for join */
				_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);
				rtw_indicate_connect(adapter);
			} else {
				RTW_INFO("try_to_join, but select scanning queue fail, to_roam:%d\n", rtw_to_roam(adapter));

				if (rtw_to_roam(adapter) != 0) {
					if (rtw_dec_to_roam(adapter) == 0
					    || _SUCCESS != rtw_sitesurvey_cmd(adapter, &pmlmepriv->assoc_ssid, 1, NULL, 0)
					   ) {
						rtw_set_to_roam(adapter, 0);
#ifdef CONFIG_INTEL_WIDI
						if (adapter->mlmepriv.widi_state == INTEL_WIDI_STATE_ROAMING) {
							memset(pmlmepriv->sa_ext, 0x00, L2SDTA_SERVICE_VE_LEN);
							intel_widi_wk_cmd(adapter, INTEL_WIDI_LISTEN_WK, NULL, 0);
							RTW_INFO("change to widi listen\n");
						}
#endif /* CONFIG_INTEL_WIDI */
						rtw_free_assoc_resources(adapter, 1);
						rtw_indicate_disconnect(adapter, 0, false);
					} else
						pmlmepriv->to_join = true;
				} else
					rtw_indicate_disconnect(adapter, 0, false);
				_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);
			}
		}
	} else {
		if (rtw_chk_roam_flags(adapter, RTW_ROAM_ACTIVE)) {
			if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)
			    && check_fwstate(pmlmepriv, _FW_LINKED)) {
				if (rtw_select_roaming_candidate(pmlmepriv) == _SUCCESS) {
#ifdef CONFIG_RTW_80211R
					if (rtw_chk_ft_flags(adapter, RTW_FT_OVER_DS_SUPPORTED)) {
						start_clnt_ft_action(adapter, (u8 *)pmlmepriv->roam_network->network.MacAddress);
					} else {
						/*wait a little time to retrieve packets buffered in the current ap while scan*/
						_set_timer(&pmlmeext->ft_roam_timer, 30);
					}
#else
					receive_disconnect(adapter, pmlmepriv->cur_network.network.MacAddress
						, WLAN_REASON_ACTIVE_ROAM, false);
#endif
				}
			}
		}
	}

	/* RTW_INFO("scan complete in %dms\n",rtw_get_passing_time_ms(pmlmepriv->scan_start_time)); */

	_exit_critical_bh(&pmlmepriv->lock, &irqL);

#ifdef CONFIG_P2P_PS
	if (check_fwstate(pmlmepriv, _FW_LINKED))
		p2p_ps_wk_cmd(adapter, P2P_PS_SCAN_DONE, 0);
#endif /* CONFIG_P2P_PS */

	rtw_mi_os_xmit_schedule(adapter);

#ifdef CONFIG_DRVEXT_MODULE_WSC
	drvext_surveydone_callback(&adapter->drvextpriv);
#endif

#ifdef DBG_CONFIG_ERROR_DETECT
	{
		struct mlme_ext_priv *pmlmeext = &adapter->mlmeextpriv;
		if (pmlmeext->sitesurvey_res.bss_cnt == 0) {
			/* rtw_hal_sreset_reset(adapter); */
		}
	}
#endif

#ifdef CONFIG_IOCTL_CFG80211
	rtw_cfg80211_surveydone_event_callback(adapter);
#endif /* CONFIG_IOCTL_CFG80211 */

	rtw_indicate_scan_done(adapter, false);

#if defined(CONFIG_CONCURRENT_MODE) && defined(CONFIG_IOCTL_CFG80211)
	rtw_cfg80211_indicate_scan_done_for_buddy(adapter, false);
#endif

}

void rtw_dummy_event_callback(_adapter *adapter , u8 *pbuf)
{

}

void rtw_fwdbg_event_callback(_adapter *adapter , u8 *pbuf)
{

}

static void free_scanqueue(struct	mlme_priv *pmlmepriv)
{
	unsigned long irqL, irqL0;
	_queue *free_queue = &pmlmepriv->free_bss_pool;
	_queue *scan_queue = &pmlmepriv->scanned_queue;
	_list	*plist, *phead, *ptemp;


	_enter_critical_bh(&scan_queue->lock, &irqL0);
	_enter_critical_bh(&free_queue->lock, &irqL);

	phead = get_list_head(scan_queue);
	plist = get_next(phead);

	while (plist != phead) {
		ptemp = get_next(plist);
		list_del_init(plist);
		list_add_tail(plist, &free_queue->queue);
		plist = ptemp;
		pmlmepriv->num_of_scanned--;
	}

	_exit_critical_bh(&free_queue->lock, &irqL);
	_exit_critical_bh(&scan_queue->lock, &irqL0);

}

static void rtw_reset_rx_info(struct debug_priv *pdbgpriv)
{
	pdbgpriv->dbg_rx_ampdu_drop_count = 0;
	pdbgpriv->dbg_rx_ampdu_forced_indicate_count = 0;
	pdbgpriv->dbg_rx_ampdu_loss_count = 0;
	pdbgpriv->dbg_rx_dup_mgt_frame_drop_count = 0;
	pdbgpriv->dbg_rx_ampdu_window_shift_cnt = 0;
}

/*
*rtw_free_assoc_resources: the caller has to lock pmlmepriv->lock
*/
void rtw_free_assoc_resources(_adapter *adapter, int lock_scanned_queue)
{
	unsigned long irqL;
	struct wlan_network *pwlan = NULL;
	struct	mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct	sta_priv *pstapriv = &adapter->stapriv;
	struct wlan_network *tgt_network = &pmlmepriv->cur_network;
	struct dvobj_priv *psdpriv = adapter->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;


#ifdef CONFIG_TDLS
	struct tdls_info *ptdlsinfo = &adapter->tdlsinfo;
#endif /* CONFIG_TDLS */


	RTW_INFO("%s-"ADPT_FMT" tgt_network MacAddress=" MAC_FMT"ssid=%s\n",
		__func__, ADPT_ARG(adapter), MAC_ARG(tgt_network->network.MacAddress), tgt_network->network.Ssid.Ssid);

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		struct sta_info *psta;

		psta = rtw_get_stainfo(&adapter->stapriv, tgt_network->network.MacAddress);

#ifdef CONFIG_TDLS
		if (ptdlsinfo->link_established) {
			rtw_tdls_cmd(adapter, NULL, TDLS_RS_RCR);
			rtw_reset_tdls_info(adapter);
			rtw_free_all_stainfo(adapter);
			/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL); */
		} else
#endif /* CONFIG_TDLS */
		{
			/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL); */
			rtw_free_stainfo(adapter,  psta);
		}

		/* _exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL); */

	}

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE)) {
		struct sta_info *psta;

		rtw_free_all_stainfo(adapter);

		psta = rtw_get_bcmc_stainfo(adapter);
		/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL);		 */
		rtw_free_stainfo(adapter, psta);
		/* _exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL);		 */

		rtw_init_bcmc_stainfo(adapter);
	}

	if (lock_scanned_queue)
		_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	pwlan = _rtw_find_same_network(&pmlmepriv->scanned_queue, tgt_network);
	if ((pwlan)  && (!check_fwstate(pmlmepriv, WIFI_UNDER_WPS))) {
		pwlan->fixed = false;

		RTW_INFO("free disconnecting network of scanned_queue\n");
		rtw_free_network_nolock(adapter, pwlan);
#ifdef CONFIG_P2P
		if (!rtw_p2p_chk_state(&adapter->wdinfo, P2P_STATE_NONE)) {
			rtw_mi_set_scan_deny(adapter, 2000);
			/* rtw_clear_scan_deny(adapter);			 */
		}
#endif /* CONFIG_P2P */
	} else {
		if (pwlan == NULL)
			RTW_INFO("free disconnecting network of scanned_queue failed due to pwlan== NULL\n\n");
		if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS))
			RTW_INFO("donot free disconnecting network of scanned_queue when WIFI_UNDER_WPS\n\n");
	}


	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) && (adapter->stapriv.asoc_sta_count == 1))
	    /*||check_fwstate(pmlmepriv, WIFI_STATION_STATE)*/) {
		if (pwlan)
			rtw_free_network_nolock(adapter, pwlan);
	}

	if (lock_scanned_queue)
		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	adapter->securitypriv.key_mask = 0;

	rtw_reset_rx_info(pdbgpriv);
}

/*
*rtw_indicate_connect: the caller has to lock pmlmepriv->lock
*/
void rtw_indicate_connect(_adapter *padapter)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;



	pmlmepriv->to_join = false;

	if (!check_fwstate(&padapter->mlmepriv, _FW_LINKED)) {

		set_fwstate(pmlmepriv, _FW_LINKED);

		rtw_led_control(padapter, LED_CTL_LINK);


#ifdef CONFIG_DRVEXT_MODULE
		if (padapter->drvextpriv.enable_wpa)
			indicate_l2_connect(padapter);
		else
#endif
		{
			rtw_os_indicate_connect(padapter);
		}

	}

	rtw_set_to_roam(padapter, 0);
#ifdef CONFIG_INTEL_WIDI
	if (padapter->mlmepriv.widi_state == INTEL_WIDI_STATE_ROAMING) {
		memset(pmlmepriv->sa_ext, 0x00, L2SDTA_SERVICE_VE_LEN);
		intel_widi_wk_cmd(padapter, INTEL_WIDI_LISTEN_WK, NULL, 0);
		RTW_INFO("change to widi listen\n");
	}
#endif /* CONFIG_INTEL_WIDI */
	if (!check_fwstate(&padapter->mlmepriv, WIFI_AP_STATE))
		rtw_mi_set_scan_deny(padapter, 3000);


}


/*
*rtw_indicate_disconnect: the caller has to lock pmlmepriv->lock
*/
void rtw_indicate_disconnect(_adapter *padapter, u16 reason, u8 locally_generated)
{
	struct	mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct mlme_ext_priv *pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX	*cur_network = &(pmlmeinfo->network);
	struct sta_info *psta;
	struct sta_priv *pstapriv = &padapter->stapriv;
	u8 *wps_ie = NULL;
	uint wpsie_len = 0;



	_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING | WIFI_UNDER_WPS);

	/* force to clear cur_network_scanned's SELECTED REGISTRAR */
	if (pmlmepriv->cur_network_scanned) {
		WLAN_BSSID_EX	*current_joined_bss = &(pmlmepriv->cur_network_scanned->network);
		if (current_joined_bss) {
			wps_ie = rtw_get_wps_ie(current_joined_bss->IEs + _FIXED_IE_LENGTH_,
				current_joined_bss->IELength - _FIXED_IE_LENGTH_, NULL, &wpsie_len);
			if (wps_ie && wpsie_len > 0) {
				u8 *attr = NULL;
				u32 attr_len;
				attr = rtw_get_wps_attr(wps_ie, wpsie_len, WPS_ATTR_SELECTED_REGISTRAR,
							NULL, &attr_len);
				if (attr)
					*(attr + 4) = 0;
			}
		}
	}
	/* RTW_INFO("clear wps when %s\n", __func__); */

	if (rtw_to_roam(padapter) > 0)
		_clr_fwstate_(pmlmepriv, _FW_LINKED);

#ifdef CONFIG_WAPI_SUPPORT
	psta = rtw_get_stainfo(pstapriv, cur_network->MacAddress);
	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE))
		rtw_wapi_return_one_sta_info(padapter, psta->hwaddr);
	else if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) ||
		 check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
		rtw_wapi_return_all_sta_info(padapter);
#endif

	if (check_fwstate(&padapter->mlmepriv, _FW_LINKED)
	    || (rtw_to_roam(padapter) <= 0)
	   ) {

		rtw_os_indicate_disconnect(padapter, reason, locally_generated);

		/* set ips_deny_time to avoid enter IPS before LPS leave */
		rtw_set_ips_deny(padapter, 3000);

		_clr_fwstate_(pmlmepriv, _FW_LINKED);

		rtw_led_control(padapter, LED_CTL_NO_LINK);

		rtw_clear_scan_deny(padapter);
	}

#ifdef CONFIG_P2P_PS
	p2p_ps_wk_cmd(padapter, P2P_PS_DISABLE, 1);
#endif /* CONFIG_P2P_PS */

#ifdef CONFIG_LPS
	rtw_lps_ctrl_wk_cmd(padapter, LPS_CTRL_DISCONNECT, 1);
#endif

#ifdef CONFIG_BEAMFORMING
	beamforming_wk_cmd(padapter, BEAMFORMING_CTRL_LEAVE, cur_network->MacAddress, ETH_ALEN, 1);
#endif /*CONFIG_BEAMFORMING*/

}

inline void rtw_indicate_scan_done(_adapter *padapter, bool aborted)
{
	RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(padapter));

	rtw_os_indicate_scan_done(padapter, aborted);

#ifdef CONFIG_IPS
	if (is_primary_adapter(padapter)
	    && (false == adapter_to_pwrctl(padapter)->bInSuspend)
	    && (check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE | WIFI_UNDER_LINKING) == false)) {
		struct pwrctrl_priv *pwrpriv;

		pwrpriv = adapter_to_pwrctl(padapter);
		rtw_set_ips_deny(padapter, 0);
#ifdef CONFIG_IPS_CHECK_IN_WD
		_set_timer(&adapter_to_dvobj(padapter)->dynamic_chk_timer, 1);
#else /* !CONFIG_IPS_CHECK_IN_WD */
		_rtw_set_pwr_state_check_timer(pwrpriv, 1);
#endif /* !CONFIG_IPS_CHECK_IN_WD */
	}
#endif /* CONFIG_IPS */
}

static u32 _rtw_wait_scan_done(_adapter *adapter, u8 abort, u32 timeout_ms)
{
	u32 start;
	u32 pass_ms;
	struct mlme_priv *pmlmepriv = &(adapter->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &(adapter->mlmeextpriv);

	start = jiffies;

	pmlmeext->scan_abort = abort;

	while (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY)
	       && rtw_get_passing_time_ms(start) <= timeout_ms) {

		if (RTW_CANNOT_RUN(adapter))
			break;

		RTW_INFO(FUNC_NDEV_FMT"fw_state=_FW_UNDER_SURVEY!\n", FUNC_NDEV_ARG(adapter->pnetdev));
		rtw_msleep_os(20);
	}

	if (abort) {
		if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY)) {
			if (!RTW_CANNOT_RUN(adapter))
				RTW_INFO(FUNC_NDEV_FMT"waiting for scan_abort time out!\n", FUNC_NDEV_ARG(adapter->pnetdev));
#ifdef CONFIG_PLATFORM_MSTAR
			/*_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY);*/
			set_survey_timer(pmlmeext, 0);
			mlme_set_scan_to_timer(pmlmepriv, 50);
#endif
			rtw_indicate_scan_done(adapter, true);
		}
	}

	pmlmeext->scan_abort = false;
	pass_ms = rtw_get_passing_time_ms(start);

	return pass_ms;

}

void rtw_scan_wait_completed(_adapter *adapter)
{
	u32 scan_to = SCANNING_TIMEOUT;

#ifdef CONFIG_SCAN_BACKOP
	if (is_supported_5g(adapter->registrypriv.wireless_mode)
	    && IsSupported24G(adapter->registrypriv.wireless_mode)) /*dual band*/
		scan_to = CONC_SCANNING_TIMEOUT_DUAL_BAND;
	else /*single band*/
		scan_to = CONC_SCANNING_TIMEOUT_SINGLE_BAND;
#endif /* CONFIG_SCAN_BACKOP */

	_rtw_wait_scan_done(adapter, false, scan_to);
}

u32 rtw_scan_abort_timeout(_adapter *adapter, u32 timeout_ms)
{
	return _rtw_wait_scan_done(adapter, true, timeout_ms);
}

void rtw_scan_abort_no_wait(_adapter *adapter)
{
	struct mlme_priv *pmlmepriv = &(adapter->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &(adapter->mlmeextpriv);

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		pmlmeext->scan_abort = true;
}

void rtw_scan_abort(_adapter *adapter)
{
	rtw_scan_abort_timeout(adapter, 200);
}

static struct sta_info *rtw_joinbss_update_stainfo(_adapter *padapter, struct wlan_network *pnetwork)
{
	int i;
	struct sta_info *bmc_sta, *psta = NULL;
	struct recv_reorder_ctrl *preorder_ctrl;
	struct sta_priv *pstapriv = &padapter->stapriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;

	psta = rtw_get_stainfo(pstapriv, pnetwork->network.MacAddress);
	if (psta == NULL)
		psta = rtw_alloc_stainfo(pstapriv, pnetwork->network.MacAddress);

	if (psta) { /* update ptarget_sta */
		RTW_INFO("%s\n", __func__);

		psta->aid  = pnetwork->join_res;

		update_sta_info(padapter, psta);

		/* update station supportRate */
		psta->bssratelen = rtw_get_rateset_len(pnetwork->network.SupportedRates);
		memcpy(psta->bssrateset, pnetwork->network.SupportedRates, psta->bssratelen);
		rtw_hal_update_sta_rate_mask(padapter, psta);

		psta->wireless_mode = pmlmeext->cur_wireless_mode;
		psta->raid = rtw_hal_networktype_to_raid(padapter, psta);


		/* sta mode */
		rtw_hal_set_odm_var(padapter, HAL_ODM_STA_INFO, psta, true);

		/* security related */
#ifdef CONFIG_RTW_80211R
		if ((padapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) && (psta->ft_pairwise_key_installed == false)) {
#else
		if (padapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) {
#endif
			padapter->securitypriv.binstallGrpkey = false;
			padapter->securitypriv.busetkipkey = false;
			padapter->securitypriv.bgrpkey_handshake = false;

			psta->ieee8021x_blocked = true;
			psta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;

			memset((u8 *)&psta->dot118021x_UncstKey, 0, sizeof(union Keytype));

			memset((u8 *)&psta->dot11tkiprxmickey, 0, sizeof(union Keytype));
			memset((u8 *)&psta->dot11tkiptxmickey, 0, sizeof(union Keytype));

			memset((u8 *)&psta->dot11txpn, 0, sizeof(union pn48));
			psta->dot11txpn.val = psta->dot11txpn.val + 1;
#ifdef CONFIG_IEEE80211W
			memset((u8 *)&psta->dot11wtxpn, 0, sizeof(union pn48));
#endif /* CONFIG_IEEE80211W */
			memset((u8 *)&psta->dot11rxpn, 0, sizeof(union pn48));
		}

		/*	Commented by Albert 2012/07/21 */
		/*	When doing the WPS, the wps_ie_len won't equal to 0 */
		/*	And the Wi-Fi driver shouldn't allow the data packet to be tramsmitted. */
		if (padapter->securitypriv.wps_ie_len != 0) {
			psta->ieee8021x_blocked = true;
			padapter->securitypriv.wps_ie_len = 0;
		}


		/* for A-MPDU Rx reordering buffer control for bmc_sta & sta_info */
		/* if A-MPDU Rx is enabled, reseting  rx_ordering_ctrl wstart_b(indicate_seq) to default value=0xffff */
		/* todo: check if AP can send A-MPDU packets */
		for (i = 0; i < 16 ; i++) {
			/* preorder_ctrl = &precvpriv->recvreorder_ctrl[i]; */
			preorder_ctrl = &psta->recvreorder_ctrl[i];
			preorder_ctrl->enable = false;
			preorder_ctrl->indicate_seq = 0xffff;
#ifdef DBG_RX_SEQ
			RTW_INFO("DBG_RX_SEQ %s:%d indicate_seq:%u\n", __func__, __LINE__,
				 preorder_ctrl->indicate_seq);
#endif
			preorder_ctrl->wend_b = 0xffff;
			preorder_ctrl->wsize_b = 64;/* max_ampdu_sz; */ /* ex. 32(kbytes) -> wsize_b=32 */
			preorder_ctrl->ampdu_size = RX_AMPDU_SIZE_INVALID;
		}


		bmc_sta = rtw_get_bcmc_stainfo(padapter);
		if (bmc_sta) {
			for (i = 0; i < 16 ; i++) {
				/* preorder_ctrl = &precvpriv->recvreorder_ctrl[i]; */
				preorder_ctrl = &bmc_sta->recvreorder_ctrl[i];
				preorder_ctrl->enable = false;
				preorder_ctrl->indicate_seq = 0xffff;
#ifdef DBG_RX_SEQ
				RTW_INFO("DBG_RX_SEQ %s:%d indicate_seq:%u\n", __func__, __LINE__,
					 preorder_ctrl->indicate_seq);
#endif
				preorder_ctrl->wend_b = 0xffff;
				preorder_ctrl->wsize_b = 64;/* max_ampdu_sz; */ /* ex. 32(kbytes) -> wsize_b=32 */
				preorder_ctrl->ampdu_size = RX_AMPDU_SIZE_INVALID;
			}
		}
	}

	return psta;

}

/* pnetwork : returns from rtw_joinbss_event_callback
 * ptarget_wlan: found from scanned_queue */
static void rtw_joinbss_update_network(_adapter *padapter, struct wlan_network *ptarget_wlan, struct wlan_network  *pnetwork)
{
	struct mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	struct wlan_network  *cur_network = &(pmlmepriv->cur_network);

	RTW_INFO("%s\n", __func__);



	/* why not use ptarget_wlan?? */
	memcpy(&cur_network->network, &pnetwork->network, pnetwork->network.Length);
	/* some IEs in pnetwork is wrong, so we should use ptarget_wlan IEs */
	cur_network->network.IELength = ptarget_wlan->network.IELength;
	memcpy(&cur_network->network.IEs[0], &ptarget_wlan->network.IEs[0], MAX_IE_SZ);

	cur_network->aid = pnetwork->join_res;


#ifdef CONFIG_NEW_SIGNAL_STAT_PROCESS
	rtw_set_signal_stat_timer(&padapter->recvpriv);
#endif
	padapter->recvpriv.signal_strength = ptarget_wlan->network.PhyInfo.SignalStrength;
	padapter->recvpriv.signal_qual = ptarget_wlan->network.PhyInfo.SignalQuality;
	/* the ptarget_wlan->network.Rssi is raw data, we use ptarget_wlan->network.PhyInfo.SignalStrength instead (has scaled) */
	padapter->recvpriv.rssi = translate_percentage_to_dbm(ptarget_wlan->network.PhyInfo.SignalStrength);
#if defined(DBG_RX_SIGNAL_DISPLAY_PROCESSING) && 1
	RTW_INFO(FUNC_ADPT_FMT" signal_strength:%3u, rssi:%3d, signal_qual:%3u"
		 "\n"
		 , FUNC_ADPT_ARG(padapter)
		 , padapter->recvpriv.signal_strength
		 , padapter->recvpriv.rssi
		 , padapter->recvpriv.signal_qual
		);
#endif
#ifdef CONFIG_NEW_SIGNAL_STAT_PROCESS
	rtw_set_signal_stat_timer(&padapter->recvpriv);
#endif

	/* update fw_state */ /* will clr _FW_UNDER_LINKING here indirectly */

	switch (pnetwork->network.InfrastructureMode) {
	case Ndis802_11Infrastructure:

		if (pmlmepriv->fw_state & WIFI_UNDER_WPS)
			/*pmlmepriv->fw_state = WIFI_STATION_STATE|WIFI_UNDER_WPS;*/
			init_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_UNDER_WPS);
		else
			/*pmlmepriv->fw_state = WIFI_STATION_STATE;*/
			init_fwstate(pmlmepriv, WIFI_STATION_STATE);
		break;
	case Ndis802_11IBSS:
		/*pmlmepriv->fw_state = WIFI_ADHOC_STATE;*/
		init_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
		break;
	default:
		/*pmlmepriv->fw_state = WIFI_NULL_STATE;*/
		init_fwstate(pmlmepriv, WIFI_NULL_STATE);
		break;
	}

	rtw_update_protection(padapter, (cur_network->network.IEs) + sizeof(NDIS_802_11_FIXED_IEs),
			      (cur_network->network.IELength));

	rtw_update_ht_cap(padapter, cur_network->network.IEs, cur_network->network.IELength, (u8) cur_network->network.Configuration.DSConfig);
}

/* Notes: the fucntion could be > passive_level (the same context as Rx tasklet)
 * pnetwork : returns from rtw_joinbss_event_callback
 * ptarget_wlan: found from scanned_queue
 * if join_res > 0, for (fw_state==WIFI_STATION_STATE), we check if  "ptarget_sta" & "ptarget_wlan" exist.
 * if join_res > 0, for (fw_state==WIFI_ADHOC_STATE), we only check if "ptarget_wlan" exist.
 * if join_res > 0, update "cur_network->network" from "pnetwork->network" if (ptarget_wlan !=NULL).
 */
/* #define REJOIN */
void rtw_joinbss_event_prehandle(_adapter *adapter, u8 *pbuf)
{
	unsigned long irqL, irqL2;
	static u8 retry = 0;
	u8 timer_cancelled;
	struct sta_info *ptarget_sta = NULL, *pcur_sta = NULL;
	struct	sta_priv *pstapriv = &adapter->stapriv;
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);
	struct wlan_network	*pnetwork	= (struct wlan_network *)pbuf;
	struct wlan_network	*cur_network = &(pmlmepriv->cur_network);
	struct wlan_network	*pcur_wlan = NULL, *ptarget_wlan = NULL;
	unsigned int		the_same_macaddr = false;

	rtw_get_encrypt_decrypt_from_registrypriv(adapter);

	the_same_macaddr = !memcmp(pnetwork->network.MacAddress, cur_network->network.MacAddress, ETH_ALEN);

	pnetwork->network.Length = get_WLAN_BSSID_EX_sz(&pnetwork->network);
	if (pnetwork->network.Length > sizeof(WLAN_BSSID_EX))
		return;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	pmlmepriv->LinkDetectInfo.TrafficTransitionCount = 0;
	pmlmepriv->LinkDetectInfo.LowPowerTransitionCount = 0;


	if (pnetwork->join_res > 0) {
		_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		retry = 0;
		if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING)) {
			/* s1. find ptarget_wlan */
			if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				if (the_same_macaddr)
					ptarget_wlan = rtw_find_network(&pmlmepriv->scanned_queue, cur_network->network.MacAddress);
				else {
					pcur_wlan = rtw_find_network(&pmlmepriv->scanned_queue, cur_network->network.MacAddress);
					if (pcur_wlan)
						pcur_wlan->fixed = false;

					pcur_sta = rtw_get_stainfo(pstapriv, cur_network->network.MacAddress);
					if (pcur_sta) {
						/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL2); */
						rtw_free_stainfo(adapter,  pcur_sta);
						/* _exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL2); */
					}

					ptarget_wlan = rtw_find_network(&pmlmepriv->scanned_queue, pnetwork->network.MacAddress);
					if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
						if (ptarget_wlan)
							ptarget_wlan->fixed = true;
					}
				}

			} else {
				ptarget_wlan = _rtw_find_same_network(&pmlmepriv->scanned_queue, pnetwork);
				if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
					if (ptarget_wlan)
						ptarget_wlan->fixed = true;
				}
			}

			/* s2. update cur_network */
			if (ptarget_wlan)
				rtw_joinbss_update_network(adapter, ptarget_wlan, pnetwork);
			else {
				RTW_PRINT("Can't find ptarget_wlan when joinbss_event callback\n");
				_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
				goto ignore_joinbss_callback;
			}


			/* s3. find ptarget_sta & update ptarget_sta after update cur_network only for station mode */
			if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
				ptarget_sta = rtw_joinbss_update_stainfo(adapter, pnetwork);
				if (ptarget_sta == NULL) {
					RTW_ERR("Can't update stainfo when joinbss_event callback\n");
					_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
					goto ignore_joinbss_callback;
				}
			}

			/* s4. indicate connect			 */
			if (MLME_IS_STA(adapter) || MLME_IS_ADHOC(adapter)) {
				pmlmepriv->cur_network_scanned = ptarget_wlan;
				rtw_indicate_connect(adapter);
			}

			/* s5. Cancle assoc_timer					 */
			_cancel_timer(&pmlmepriv->assoc_timer, &timer_cancelled);


		} else {
			_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			goto ignore_joinbss_callback;
		}

		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	} else if (pnetwork->join_res == -4) {
		rtw_reset_securitypriv(adapter);
		_set_timer(&pmlmepriv->assoc_timer, 1);

		/* rtw_free_assoc_resources(adapter, 1); */

		if ((check_fwstate(pmlmepriv, _FW_UNDER_LINKING))) {
			_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);
		}

	} else { /* if join_res < 0 (join fails), then try again */

#ifdef REJOIN
		res = _FAIL;
		if (retry < 2) {
			res = rtw_select_and_join_from_scanned_queue(pmlmepriv);
		}

		if (res == _SUCCESS) {
			/* extend time of assoc_timer */
			_set_timer(&pmlmepriv->assoc_timer, MAX_JOIN_TIMEOUT);
			retry++;
		} else if (res == 2) { /* there is no need to wait for join */
			_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);
			rtw_indicate_connect(adapter);
		} else {
#endif

			_set_timer(&pmlmepriv->assoc_timer, 1);
			/* rtw_free_assoc_resources(adapter, 1); */
			_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);

#ifdef REJOIN
			retry = 0;
		}
#endif
	}

ignore_joinbss_callback:
	_exit_critical_bh(&pmlmepriv->lock, &irqL);
}

void rtw_joinbss_event_callback(_adapter *adapter, u8 *pbuf)
{
	struct wlan_network	*pnetwork	= (struct wlan_network *)pbuf;


	mlmeext_joinbss_event_callback(adapter, pnetwork->join_res);

	rtw_mi_os_xmit_schedule(adapter);

}

void rtw_sta_media_status_rpt(_adapter *adapter, struct sta_info *sta, bool connected)
{
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;
	bool miracast_enabled = 0;
	bool miracast_sink = 0;
	u8 role = H2C_MSR_ROLE_RSVD;

	if (sta == NULL) {
		RTW_PRINT(FUNC_ADPT_FMT" sta is NULL\n"
			  , FUNC_ADPT_ARG(adapter));
		rtw_warn_on(1);
		return;
	}

	if (sta->mac_id >= macid_ctl->num) {
		RTW_PRINT(FUNC_ADPT_FMT" invalid macid:%u\n"
			  , FUNC_ADPT_ARG(adapter), sta->mac_id);
		rtw_warn_on(1);
		return;
	}

	if (!rtw_macid_is_used(macid_ctl, sta->mac_id)) {
		RTW_PRINT(FUNC_ADPT_FMT" macid:%u not is used, set connected to 0\n"
			  , FUNC_ADPT_ARG(adapter), sta->mac_id);
		connected = 0;
		rtw_warn_on(1);
	}

	if (connected && !rtw_macid_is_bmc(macid_ctl, sta->mac_id)) {
		miracast_enabled = STA_OP_WFD_MODE(sta) != 0 && is_miracast_enabled(adapter);
		miracast_sink = miracast_enabled && (STA_OP_WFD_MODE(sta) & MIRACAST_SINK);

#ifdef CONFIG_TDLS
		if (sta->tdls_sta_state & TDLS_LINKED_STATE)
			role = H2C_MSR_ROLE_TDLS;
		else
#endif
			if (MLME_IS_STA(adapter)) {
				if (MLME_IS_GC(adapter))
					role = H2C_MSR_ROLE_GO;
				else
					role = H2C_MSR_ROLE_AP;
			} else if (MLME_IS_AP(adapter)) {
				if (MLME_IS_GO(adapter))
					role = H2C_MSR_ROLE_GC;
				else
					role = H2C_MSR_ROLE_STA;
			} else if (MLME_IS_ADHOC(adapter) || MLME_IS_ADHOC_MASTER(adapter))
				role = H2C_MSR_ROLE_ADHOC;

#ifdef CONFIG_WFD
		if (role == H2C_MSR_ROLE_GC
		    || role == H2C_MSR_ROLE_GO
		    || role == H2C_MSR_ROLE_TDLS
		   ) {
			if (adapter->wfd_info.rtsp_ctrlport
			    || adapter->wfd_info.tdls_rtsp_ctrlport
			    || adapter->wfd_info.peer_rtsp_ctrlport)
				rtw_wfd_st_switch(sta, 1);
		}
#endif
	}

	rtw_hal_set_FwMediaStatusRpt_single_cmd(adapter
						, connected
						, miracast_enabled
						, miracast_sink
						, role
						, sta->mac_id
					       );
}

u8 rtw_sta_media_status_rpt_cmd(_adapter *adapter, struct sta_info *sta, bool connected)
{
	struct cmd_priv	*cmdpriv = &adapter->cmdpriv;
	struct cmd_obj *cmdobj;
	struct drvextra_cmd_parm *cmd_parm;
	struct sta_media_status_rpt_cmd_parm *rpt_parm;
	u8	res = _SUCCESS;

	cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (cmdobj == NULL) {
		res = _FAIL;
		goto exit;
	}

	cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (cmd_parm == NULL) {
		rtw_mfree((u8 *)cmdobj, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	rpt_parm = (struct sta_media_status_rpt_cmd_parm *)rtw_zmalloc(sizeof(struct sta_media_status_rpt_cmd_parm));
	if (rpt_parm == NULL) {
		rtw_mfree((u8 *)cmdobj, sizeof(struct cmd_obj));
		rtw_mfree((u8 *)cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	rpt_parm->sta = sta;
	rpt_parm->connected = connected;

	cmd_parm->ec_id = STA_MSTATUS_RPT_WK_CID;
	cmd_parm->type = 0;
	cmd_parm->size = sizeof(struct sta_media_status_rpt_cmd_parm);
	cmd_parm->pbuf = (u8 *)rpt_parm;
	init_h2fwcmd_w_parm_no_rsp(cmdobj, cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(cmdpriv, cmdobj);

exit:
	return res;
}

inline void rtw_sta_media_status_rpt_cmd_hdl(_adapter *adapter, struct sta_media_status_rpt_cmd_parm *parm)
{
	rtw_sta_media_status_rpt(adapter, parm->sta, parm->connected);
}

void rtw_stassoc_event_callback(_adapter *adapter, u8 *pbuf)
{
	unsigned long irqL;
	struct sta_info *psta;
	struct mlme_priv *pmlmepriv = &(adapter->mlmepriv);
	struct stassoc_event	*pstassoc	= (struct stassoc_event *)pbuf;
	struct wlan_network	*cur_network = &(pmlmepriv->cur_network);
	struct wlan_network	*ptarget_wlan = NULL;


#if CONFIG_RTW_MACADDR_ACL
	if (rtw_access_ctrl(adapter, pstassoc->macaddr) == false)
		return;
#endif

#if defined(CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME)
	if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		psta = rtw_get_stainfo(&adapter->stapriv, pstassoc->macaddr);
		if (psta) {
			u8 *passoc_req = NULL;
			u32 assoc_req_len = 0;

			rtw_sta_media_status_rpt(adapter, psta, 1);

#ifndef CONFIG_AUTO_AP_MODE

			ap_sta_info_defer_update(adapter, psta);

			/* report to upper layer */
			RTW_INFO("indicate_sta_assoc_event to upper layer - hostapd\n");
#ifdef CONFIG_IOCTL_CFG80211
			_enter_critical_bh(&psta->lock, &irqL);
			if (psta->passoc_req && psta->assoc_req_len > 0) {
				passoc_req = rtw_zmalloc(psta->assoc_req_len);
				if (passoc_req) {
					assoc_req_len = psta->assoc_req_len;
					memcpy(passoc_req, psta->passoc_req, assoc_req_len);

					rtw_mfree(psta->passoc_req , psta->assoc_req_len);
					psta->passoc_req = NULL;
					psta->assoc_req_len = 0;
				}
			}
			_exit_critical_bh(&psta->lock, &irqL);

			if (passoc_req && assoc_req_len > 0) {
				rtw_cfg80211_indicate_sta_assoc(adapter, passoc_req, assoc_req_len);

				rtw_mfree(passoc_req, assoc_req_len);
			}
#else /* !CONFIG_IOCTL_CFG80211	 */
			rtw_indicate_sta_assoc_event(adapter, psta);
#endif /* !CONFIG_IOCTL_CFG80211 */
#endif /* !CONFIG_AUTO_AP_MODE */

#ifdef CONFIG_BEAMFORMING
			beamforming_wk_cmd(adapter, BEAMFORMING_CTRL_ENTER, (u8 *)psta, sizeof(struct sta_info), 0);
#endif/*CONFIG_BEAMFORMING*/
			if (is_wep_enc(adapter->securitypriv.dot11PrivacyAlgrthm))
				rtw_ap_wep_pk_setting(adapter, psta);
		}
		goto exit;
	}
#endif /* defined (CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME) */

	/* for AD-HOC mode */
	psta = rtw_get_stainfo(&adapter->stapriv, pstassoc->macaddr);
	if (psta == NULL) {
		RTW_ERR(FUNC_ADPT_FMT" get no sta_info with "MAC_FMT"\n"
			, FUNC_ADPT_ARG(adapter), MAC_ARG(pstassoc->macaddr));
		rtw_warn_on(1);
		goto exit;
	}

	rtw_hal_set_odm_var(adapter, HAL_ODM_STA_INFO, psta, true);

	rtw_sta_media_status_rpt(adapter, psta, 1);

	if (adapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X)
		psta->dot118021XPrivacy = adapter->securitypriv.dot11PrivacyAlgrthm;


	psta->ieee8021x_blocked = false;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE))) {
		if (adapter->stapriv.asoc_sta_count == 2) {
			_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			ptarget_wlan = rtw_find_network(&pmlmepriv->scanned_queue, cur_network->network.MacAddress);
			pmlmepriv->cur_network_scanned = ptarget_wlan;
			if (ptarget_wlan)
				ptarget_wlan->fixed = true;
			_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			/* a sta + bc/mc_stainfo (not Ibss_stainfo) */
			rtw_indicate_connect(adapter);
		}
	}

	_exit_critical_bh(&pmlmepriv->lock, &irqL);


	mlmeext_sta_add_event_callback(adapter, psta);
exit:
	return;
}

#ifdef CONFIG_IEEE80211W
void rtw_sta_timeout_event_callback(_adapter *adapter, u8 *pbuf)
{
	unsigned long irqL;
	struct sta_info *psta;
	struct stadel_event *pstadel = (struct stadel_event *)pbuf;
	struct sta_priv *pstapriv = &adapter->stapriv;


	psta = rtw_get_stainfo(&adapter->stapriv, pstadel->macaddr);

	if (psta) {
		u8 updated = false;

		_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
		if (list_empty(&psta->asoc_list) == false) {
			list_del_init(&psta->asoc_list);
			pstapriv->asoc_list_cnt--;
			updated = ap_free_sta(adapter, psta, true, WLAN_REASON_PREV_AUTH_NOT_VALID, true);
		}
		_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

		associated_clients_update(adapter, updated, STA_INFO_UPDATE_ALL);
	}



}
#endif /* CONFIG_IEEE80211W */

#ifdef CONFIG_RTW_80211R
void rtw_update_ft_stainfo(_adapter *padapter, WLAN_BSSID_EX *pnetwork)
{
	struct sta_priv		*pstapriv = &padapter->stapriv;
	struct sta_info		*psta = NULL;

	psta = rtw_get_stainfo(pstapriv, pnetwork->MacAddress);
	if (psta == NULL)
		psta = rtw_alloc_stainfo(pstapriv, pnetwork->MacAddress);

	if (padapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) {

		padapter->securitypriv.binstallGrpkey = false;
		padapter->securitypriv.busetkipkey = false;
		padapter->securitypriv.bgrpkey_handshake = false;

		psta->ieee8021x_blocked = true;
		psta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;
		psta->dot11txpn.val = psta->dot11txpn.val + 1;

		memset((u8 *)&psta->dot118021x_UncstKey, 0, sizeof(union Keytype));
		memset((u8 *)&psta->dot11tkiprxmickey, 0, sizeof(union Keytype));
		memset((u8 *)&psta->dot11tkiptxmickey, 0, sizeof(union Keytype));
		memset((u8 *)&psta->dot11txpn, 0, sizeof(union pn48));
#ifdef CONFIG_IEEE80211W
		memset((u8 *)&psta->dot11wtxpn, 0, sizeof(union pn48));
#endif
		memset((u8 *)&psta->dot11rxpn, 0, sizeof(union pn48));
	}

}

void rtw_ft_reassoc_event_callback(_adapter *padapter, u8 *pbuf)
{
	struct mlme_priv		*pmlmepriv = &(padapter->mlmepriv);
	struct stassoc_event	*pstassoc = (struct stassoc_event *)pbuf;
	ft_priv	*pftpriv = &pmlmepriv->ftpriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX		*pnetwork = (WLAN_BSSID_EX *)(&(pmlmeinfo->network));
	struct cfg80211_ft_event_params ft_evt_parms;
	unsigned long irqL;

	memset(&ft_evt_parms, 0, sizeof(ft_evt_parms));
	rtw_update_ft_stainfo(padapter, pnetwork);
	ft_evt_parms.ies_len = pftpriv->ft_event.ies_len;
	ft_evt_parms.ies =  rtw_zmalloc(ft_evt_parms.ies_len);
	if (ft_evt_parms.ies)
		memcpy((void *)ft_evt_parms.ies, pftpriv->ft_event.ies, ft_evt_parms.ies_len);
	 else
		goto err_2;

	ft_evt_parms.target_ap = rtw_zmalloc(ETH_ALEN);
	if (ft_evt_parms.target_ap)
		memcpy((void *)ft_evt_parms.target_ap, pstassoc->macaddr, ETH_ALEN);
	else
		goto err_1;

	ft_evt_parms.ric_ies = pftpriv->ft_event.ric_ies;
	ft_evt_parms.ric_ies_len = pftpriv->ft_event.ric_ies_len;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	rtw_set_ft_status(padapter, RTW_FT_AUTHENTICATED_STA);
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

	rtw_cfg80211_ft_event(padapter, &ft_evt_parms);
	RTW_INFO("%s: to "MAC_FMT"\n", __func__, MAC_ARG(ft_evt_parms.target_ap));

	rtw_mfree((u8 *)pftpriv->ft_event.target_ap, ETH_ALEN);
err_1:
	rtw_mfree((u8 *)ft_evt_parms.ies, ft_evt_parms.ies_len);
err_2:
	return;
}
#endif

static void rtw_sta_mstatus_disc_rpt(_adapter *adapter, u8 mac_id)
{
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;

	RTW_INFO("%s "ADPT_FMT" - mac_id=%d\n", __func__, ADPT_ARG(adapter), mac_id);

	if (mac_id >= 0 && mac_id < macid_ctl->num) {
		rtw_hal_set_FwMediaStatusRpt_single_cmd(adapter, 0, 0, 0, 0, mac_id);
		/*
		 * For safety, prevent from keeping macid sleep.
		 * If we can sure all power mode enter/leave are paired,
		 * this check can be removed.
		 * Lucas@20131113
		 */
		/* wakeup macid after disconnect. */
		/*if (MLME_IS_STA(adapter))*/
		rtw_hal_macid_wakeup(adapter, mac_id);
	} else {
		RTW_PRINT(FUNC_ADPT_FMT" invalid macid:%u\n"
			  , FUNC_ADPT_ARG(adapter), mac_id);
		rtw_warn_on(1);
	}
}
void rtw_sta_mstatus_report(_adapter *adapter)
{
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct wlan_network *tgt_network = &pmlmepriv->cur_network;
	struct sta_info *psta = NULL;

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		psta = rtw_get_stainfo(&adapter->stapriv, tgt_network->network.MacAddress);
		if (psta)
			rtw_sta_mstatus_disc_rpt(adapter, psta->mac_id);
		else
			RTW_INFO("%s "ADPT_FMT" - mac_addr: "MAC_FMT" psta == NULL\n", __func__, ADPT_ARG(adapter), MAC_ARG(tgt_network->network.MacAddress));
	}
}

void rtw_stadel_event_callback(_adapter *adapter, u8 *pbuf)
{
	unsigned long irqL, irqL2;

	struct sta_info *psta;
	struct wlan_network *pwlan = NULL;
	WLAN_BSSID_EX    *pdev_network = NULL;
	u8 *pibss = NULL;
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);
	struct	stadel_event *pstadel	= (struct stadel_event *)pbuf;
	struct	sta_priv *pstapriv = &adapter->stapriv;
	struct wlan_network *tgt_network = &(pmlmepriv->cur_network);
	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);


	RTW_INFO("%s(mac_id=%d)=" MAC_FMT "\n", __func__, pstadel->mac_id, MAC_ARG(pstadel->macaddr));
	rtw_sta_mstatus_disc_rpt(adapter, pstadel->mac_id);

	psta = rtw_get_stainfo(&adapter->stapriv, pstadel->macaddr);

	if (psta == NULL) {
		RTW_INFO("%s(mac_id=%d)=" MAC_FMT " psta == NULL\n", __func__, pstadel->mac_id, MAC_ARG(pstadel->macaddr));
		/*rtw_warn_on(1);*/
	}

	if (psta)
		rtw_wfd_st_switch(psta, 0);

	/* if(check_fwstate(pmlmepriv, WIFI_AP_STATE)) */
	if ((pmlmeinfo->state & 0x03) == WIFI_FW_AP_STATE) {
#ifdef CONFIG_IOCTL_CFG80211
#ifdef COMPAT_KERNEL_RELEASE

#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) || defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER)
		rtw_cfg80211_indicate_sta_disassoc(adapter, pstadel->macaddr, *(u16 *)pstadel->rsvd);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) || defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER) */
#endif /* CONFIG_IOCTL_CFG80211 */

		return;
	}

	mlmeext_sta_del_event_callback(adapter);

	_enter_critical_bh(&pmlmepriv->lock, &irqL2);

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		u16 reason = *((unsigned short *)(pstadel->rsvd));
		bool roam = false;
		struct wlan_network *roam_target = NULL;

#ifdef CONFIG_LAYER2_ROAMING
#ifdef CONFIG_RTW_80211R
		if (reason == WLAN_REASON_EXPIRATION_CHK && rtw_chk_roam_flags(adapter, RTW_ROAM_ON_EXPIRED))
			pmlmepriv->ftpriv.ft_roam_on_expired = true;
		else
			pmlmepriv->ftpriv.ft_roam_on_expired = false;
#endif
		if (adapter->registrypriv.wifi_spec == 1)
			roam = false;
		else if (reason == WLAN_REASON_EXPIRATION_CHK && rtw_chk_roam_flags(adapter, RTW_ROAM_ON_EXPIRED))
			roam = true;
		else if (reason == WLAN_REASON_ACTIVE_ROAM && rtw_chk_roam_flags(adapter, RTW_ROAM_ACTIVE)) {
			roam = true;
			roam_target = pmlmepriv->roam_network;
		}
#ifdef CONFIG_INTEL_WIDI
		else if (adapter->mlmepriv.widi_state == INTEL_WIDI_STATE_CONNECTED)
			roam = true;
#endif /* CONFIG_INTEL_WIDI */

		if (roam) {
			if (rtw_to_roam(adapter) > 0)
				rtw_dec_to_roam(adapter); /* this stadel_event is caused by roaming, decrease to_roam */
			else if (rtw_to_roam(adapter) == 0)
				rtw_set_to_roam(adapter, adapter->registrypriv.max_roaming_times);
		} else
			rtw_set_to_roam(adapter, 0);
#endif /* CONFIG_LAYER2_ROAMING */

		rtw_free_uc_swdec_pending_queue(adapter);

		rtw_free_assoc_resources(adapter, 1);
		rtw_free_mlme_priv_ie_data(pmlmepriv);

		_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		/* remove the network entry in scanned_queue */
		pwlan = rtw_find_network(&pmlmepriv->scanned_queue, tgt_network->network.MacAddress);
		if ((pwlan)  && (!check_fwstate(pmlmepriv, WIFI_UNDER_WPS))) {
			pwlan->fixed = false;
			rtw_free_network_nolock(adapter, pwlan);
		}
		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

		rtw_indicate_disconnect(adapter, *(u16 *)pstadel->rsvd, pstadel->locally_generated);
#ifdef CONFIG_INTEL_WIDI
		if (!rtw_to_roam(adapter))
			process_intel_widi_disconnect(adapter, 1);
#endif /* CONFIG_INTEL_WIDI */

		_rtw_roaming(adapter, roam_target);
	}

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) ||
	    check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) {

		/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL); */
		rtw_free_stainfo(adapter,  psta);
		/* _exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL); */

		if (adapter->stapriv.asoc_sta_count == 1) { /* a sta + bc/mc_stainfo (not Ibss_stainfo) */
			/* rtw_indicate_disconnect(adapter); */ /* removed@20091105 */
			_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			/* free old ibss network */
			/* pwlan = rtw_find_network(&pmlmepriv->scanned_queue, pstadel->macaddr); */
			pwlan = rtw_find_network(&pmlmepriv->scanned_queue, tgt_network->network.MacAddress);
			if (pwlan) {
				pwlan->fixed = false;
				rtw_free_network_nolock(adapter, pwlan);
			}
			_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			/* re-create ibss */
			pdev_network = &(adapter->registrypriv.dev_network);
			pibss = adapter->registrypriv.dev_network.MacAddress;

			memcpy(pdev_network, &tgt_network->network, get_WLAN_BSSID_EX_sz(&tgt_network->network));

			memset(&pdev_network->Ssid, 0, sizeof(NDIS_802_11_SSID));
			memcpy(&pdev_network->Ssid, &pmlmepriv->assoc_ssid, sizeof(NDIS_802_11_SSID));

			rtw_update_registrypriv_dev_network(adapter);

			rtw_generate_random_ibss(pibss);

			if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) {
//				set_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE);
				_clr_fwstate_(pmlmepriv, WIFI_ADHOC_STATE);
			}

			if (rtw_create_ibss_cmd(adapter, 0) != _SUCCESS)
				RTW_ERR("rtw_create_ibss_cmd FAIL\n");

		}

	}

	_exit_critical_bh(&pmlmepriv->lock, &irqL2);


}


void rtw_cpwm_event_callback(PADAPTER padapter, u8 *pbuf)
{
#ifdef CONFIG_LPS_LCLK
	struct reportpwrstate_parm *preportpwrstate;
#endif


#ifdef CONFIG_LPS_LCLK
	preportpwrstate = (struct reportpwrstate_parm *)pbuf;
	preportpwrstate->state |= (u8)(adapter_to_pwrctl(padapter)->cpwm_tog + 0x80);
	cpwm_int_hdl(padapter, preportpwrstate);
#endif


}


void rtw_wmm_event_callback(PADAPTER padapter, u8 *pbuf)
{

	WMMOnAssocRsp(padapter);


}

/*
* _rtw_join_timeout_handler - Timeout/faliure handler for CMD JoinBss
* @adapter: pointer to _adapter structure
*/
void _rtw_join_timeout_handler(_adapter *adapter)
{
	unsigned long irqL;
	struct	mlme_priv *pmlmepriv = &adapter->mlmepriv;

	RTW_INFO("%s, fw_state=%x\n", __func__, get_fwstate(pmlmepriv));

	if (RTW_CANNOT_RUN(adapter))
		return;


	_enter_critical_bh(&pmlmepriv->lock, &irqL);

#ifdef CONFIG_LAYER2_ROAMING
	if (rtw_to_roam(adapter) > 0) { /* join timeout caused by roaming */
		while (1) {
			rtw_dec_to_roam(adapter);
			if (rtw_to_roam(adapter) != 0) { /* try another */
				int do_join_r;
				RTW_INFO("%s try another roaming\n", __func__);
				do_join_r = rtw_do_join(adapter);
				if (_SUCCESS != do_join_r) {
					RTW_INFO("%s roaming do_join return %d\n", __func__ , do_join_r);
					continue;
				}
				break;
			} else {
#ifdef CONFIG_INTEL_WIDI
				if (adapter->mlmepriv.widi_state == INTEL_WIDI_STATE_ROAMING) {
					memset(pmlmepriv->sa_ext, 0x00, L2SDTA_SERVICE_VE_LEN);
					intel_widi_wk_cmd(adapter, INTEL_WIDI_LISTEN_WK, NULL, 0);
					RTW_INFO("change to widi listen\n");
				}
#endif /* CONFIG_INTEL_WIDI */
				RTW_INFO("%s We've try roaming but fail\n", __func__);
#ifdef CONFIG_RTW_80211R
				rtw_clr_ft_flags(adapter, RTW_FT_SUPPORTED|RTW_FT_OVER_DS_SUPPORTED);
				rtw_reset_ft_status(adapter);
#endif
				rtw_indicate_disconnect(adapter, 0, false);
				break;
			}
		}

	} else
#endif
	{
		rtw_indicate_disconnect(adapter, 0, false);
		free_scanqueue(pmlmepriv);/* ??? */

#ifdef CONFIG_IOCTL_CFG80211
		/* indicate disconnect for the case that join_timeout and check_fwstate != FW_LINKED */
		rtw_cfg80211_indicate_disconnect(adapter, 0, false);
#endif /* CONFIG_IOCTL_CFG80211 */

	}

	_exit_critical_bh(&pmlmepriv->lock, &irqL);


#ifdef CONFIG_DRVEXT_MODULE_WSC
	drvext_assoc_fail_indicate(&adapter->drvextpriv);
#endif



}

/*
* rtw_scan_timeout_handler - Timeout/Faliure handler for CMD SiteSurvey
* @adapter: pointer to _adapter structure
*/
void rtw_scan_timeout_handler(_adapter *adapter)
{
	unsigned long irqL;
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	RTW_INFO(FUNC_ADPT_FMT" fw_state=%x\n", FUNC_ADPT_ARG(adapter), get_fwstate(pmlmepriv));

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY);

	_exit_critical_bh(&pmlmepriv->lock, &irqL);

#ifdef CONFIG_IOCTL_CFG80211
	rtw_cfg80211_surveydone_event_callback(adapter);
#endif /* CONFIG_IOCTL_CFG80211 */

	rtw_indicate_scan_done(adapter, true);

#if defined(CONFIG_CONCURRENT_MODE) && defined(CONFIG_IOCTL_CFG80211)
	rtw_cfg80211_indicate_scan_done_for_buddy(adapter, true);
#endif
}

void rtw_mlme_reset_auto_scan_int(_adapter *adapter, u8 *reason)
{
	struct mlme_priv *mlme = &adapter->mlmepriv;
	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8 u_ch;
	u32 interval_ms = 0xffffffff; /* 0xffffffff: special value to make min() works well, also means no auto scan */

	*reason = RTW_AUTO_SCAN_REASON_UNSPECIFIED;
	rtw_mi_get_ch_setting_union(adapter, &u_ch, NULL, NULL);

	if (hal_chk_bw_cap(adapter, BW_CAP_40M)
	    && is_client_associated_to_ap(adapter)
	    && u_ch >= 1 && u_ch <= 14
	    && adapter->registrypriv.wifi_spec
	    /* TODO: AP Connected is 40MHz capability? */
	   ) {
		interval_ms = rtw_min(interval_ms, 60 * 1000);
		*reason |= RTW_AUTO_SCAN_REASON_2040_BSS;
	}

exit:
	if (interval_ms == 0xffffffff)
		interval_ms = 0;

	rtw_mlme_set_auto_scan_int(adapter, interval_ms);
	return;
}

void rtw_drv_scan_by_self(_adapter *padapter, u8 reason)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct rtw_ieee80211_channel ch_for_2040_bss[14] = {
		{1, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{2, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{3, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{4, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{5, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{6, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{7, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{8, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{9, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{10, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{11, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{12, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{13, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
		{14, RTW_IEEE80211_CHAN_PASSIVE_SCAN},
	};
	struct rtw_ieee80211_channel *ch_sel = NULL;
	int ch_num = 0;

	if (rtw_is_scan_deny(padapter))
		goto exit;

	if (!rtw_is_adapter_up(padapter))
		goto exit;

	if (rtw_mi_busy_traffic_check(padapter, false)) {
#ifdef CONFIG_LAYER2_ROAMING
		if (rtw_chk_roam_flags(padapter, RTW_ROAM_ACTIVE) && pmlmepriv->need_to_roam) {
			RTW_INFO("need to roam, don't care BusyTraffic\n");
		} else
#endif
		{
			RTW_INFO(FUNC_ADPT_FMT" exit BusyTraffic\n", FUNC_ADPT_ARG(padapter));
			goto exit;
		}
	}
	if (check_fwstate(pmlmepriv, WIFI_AP_STATE) && check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		RTW_INFO(FUNC_ADPT_FMT" WIFI_AP_STATE && WIFI_UNDER_WPS\n", FUNC_ADPT_ARG(padapter));
		goto exit;
	}
	if (check_fwstate(pmlmepriv, (_FW_UNDER_SURVEY | _FW_UNDER_LINKING))) {
		RTW_INFO(FUNC_ADPT_FMT" _FW_UNDER_SURVEY|_FW_UNDER_LINKING\n", FUNC_ADPT_ARG(padapter));
		goto exit;
	}

#ifdef CONFIG_CONCURRENT_MODE
	if (rtw_mi_buddy_check_fwstate(padapter, (_FW_UNDER_SURVEY | _FW_UNDER_LINKING | WIFI_UNDER_WPS))) {
		RTW_INFO(FUNC_ADPT_FMT", but buddy_intf is under scanning or linking or wps_phase\n", FUNC_ADPT_ARG(padapter));
		goto exit;
	}
#endif

	RTW_INFO(FUNC_ADPT_FMT" reason:0x%02x\n", FUNC_ADPT_ARG(padapter), reason);

	/* only for 20/40 BSS */
	if (reason == RTW_AUTO_SCAN_REASON_2040_BSS) {
		ch_sel = ch_for_2040_bss;
		ch_num = 14;
	}

	rtw_set_802_11_bssid_list_scan(padapter, NULL, 0, ch_sel, ch_num);
exit:
	return;
}

static void rtw_auto_scan_handler(_adapter *padapter)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	u8 reason = RTW_AUTO_SCAN_REASON_UNSPECIFIED;

	rtw_mlme_reset_auto_scan_int(padapter, &reason);

#ifdef CONFIG_P2P
	if (!rtw_p2p_chk_state(&padapter->wdinfo, P2P_STATE_NONE))
		goto exit;
#endif

#ifdef CONFIG_TDLS
	if (padapter->tdlsinfo.link_established)
		goto exit;
#endif

	if (pmlmepriv->auto_scan_int_ms == 0
	    || rtw_get_passing_time_ms(pmlmepriv->scan_start_time) < pmlmepriv->auto_scan_int_ms)
		goto exit;

	rtw_drv_scan_by_self(padapter, reason);

exit:
	return;
}
static u8 is_drv_in_lps(_adapter *adapter)
{
	u8 is_in_lps = false;

	#ifdef CONFIG_LPS_LCLK_WD_TIMER /* to avoid leaving lps 32k frequently*/
	if ((adapter_to_pwrctl(adapter)->bFwCurrentInPSMode)
	#ifdef CONFIG_BT_COEXIST
		&& (rtw_btcoex_IsBtControlLps(adapter) == false)
	#endif
		)
		is_in_lps = true;
	#endif /* CONFIG_LPS_LCLK_WD_TIMER*/
	return is_in_lps;
}
void rtw_iface_dynamic_check_timer_handler(_adapter *adapter)
{
#ifdef CONFIG_AP_MODE
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
#endif /* CONFIG_AP_MODE */

	if (adapter->net_closed)
		return;
	#ifdef CONFIG_LPS_LCLK_WD_TIMER /* to avoid leaving lps 32k frequently*/
	if (is_drv_in_lps(adapter)) {
		u8 bEnterPS;

		linked_status_chk(adapter, 1);

		bEnterPS = traffic_status_watchdog(adapter, 1);
		if (bEnterPS) {
			/* rtw_lps_ctrl_wk_cmd(adapter, LPS_CTRL_ENTER, 1); */
			rtw_hal_dm_watchdog_in_lps(adapter);
		} else {
			/* call rtw_lps_ctrl_wk_cmd(padapter, LPS_CTRL_LEAVE, 1) in traffic_status_watchdog() */
		}
	}
	#endif /* CONFIG_LPS_LCLK_WD_TIMER	*/

	/* auto site survey */
	rtw_auto_scan_handler(adapter);

#ifndef CONFIG_ACTIVE_KEEP_ALIVE_CHECK
#ifdef CONFIG_AP_MODE
	if (check_fwstate(pmlmepriv, WIFI_AP_STATE))
		expire_timeout_chk(adapter);
#endif
#endif /* !CONFIG_ACTIVE_KEEP_ALIVE_CHECK */

#ifdef CONFIG_BR_EXT

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35))
	rcu_read_lock();
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)) */

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
	if (adapter->pnetdev->br_port
#else	/* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
	if (rcu_dereference(adapter->pnetdev->rx_handler_data)
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
		&& (check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_ADHOC_STATE))) {
		/* expire NAT2.5 entry */
		nat25_db_expire(adapter);

		if (adapter->pppoe_connection_in_progress > 0)
			adapter->pppoe_connection_in_progress--;
		/* due to rtw_dynamic_check_timer_handler() is called every 2 seconds */
		if (adapter->pppoe_connection_in_progress > 0)
			adapter->pppoe_connection_in_progress--;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35))
	rcu_read_unlock();
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)) */

#endif /* CONFIG_BR_EXT */

}

/*#define DBG_TRAFFIC_STATISTIC*/
static void collect_traffic_statistics(_adapter *padapter)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	/*memset(&pdvobjpriv->traffic_stat, 0, sizeof(struct rtw_traffic_statistics));*/

	/* Tx bytes reset*/
	pdvobjpriv->traffic_stat.tx_bytes = 0;
	pdvobjpriv->traffic_stat.tx_pkts = 0;
	pdvobjpriv->traffic_stat.tx_drop = 0;

	/* Rx bytes reset*/
	pdvobjpriv->traffic_stat.rx_bytes = 0;
	pdvobjpriv->traffic_stat.rx_pkts = 0;
	pdvobjpriv->traffic_stat.rx_drop = 0;

	rtw_mi_traffic_statistics(padapter);

	/* Calculate throughput in last interval */
	pdvobjpriv->traffic_stat.cur_tx_bytes = pdvobjpriv->traffic_stat.tx_bytes - pdvobjpriv->traffic_stat.last_tx_bytes;
	pdvobjpriv->traffic_stat.cur_rx_bytes = pdvobjpriv->traffic_stat.rx_bytes - pdvobjpriv->traffic_stat.last_rx_bytes;
	pdvobjpriv->traffic_stat.last_tx_bytes = pdvobjpriv->traffic_stat.tx_bytes;
	pdvobjpriv->traffic_stat.last_rx_bytes = pdvobjpriv->traffic_stat.rx_bytes;

	pdvobjpriv->traffic_stat.cur_tx_tp = (u32)(pdvobjpriv->traffic_stat.cur_tx_bytes * 8 / 2 / 1024 / 1024);
	pdvobjpriv->traffic_stat.cur_rx_tp = (u32)(pdvobjpriv->traffic_stat.cur_rx_bytes * 8 / 2 / 1024 / 1024);

	#ifdef DBG_TRAFFIC_STATISTIC
	RTW_INFO("\n========================\n");
	RTW_INFO("cur_tx_bytes:%lld\n", pdvobjpriv->traffic_stat.cur_tx_bytes);
	RTW_INFO("cur_rx_bytes:%lld\n", pdvobjpriv->traffic_stat.cur_rx_bytes);

	RTW_INFO("last_tx_bytes:%lld\n", pdvobjpriv->traffic_stat.last_tx_bytes);
	RTW_INFO("last_rx_bytes:%lld\n", pdvobjpriv->traffic_stat.last_rx_bytes);

	RTW_INFO("cur_tx_tp:%d\n", pdvobjpriv->traffic_stat.cur_tx_tp);
	RTW_INFO("cur_rx_tp:%d\n", pdvobjpriv->traffic_stat.cur_rx_tp);
	#endif
}

void rtw_dynamic_check_timer_handler(_adapter *adapter)
{
	if (!adapter)
		return;

	if (!rtw_is_hw_init_completed(adapter))
		return;

	if (RTW_CANNOT_RUN(adapter))
		return;

	collect_traffic_statistics(adapter);

	rtw_mi_dynamic_check_timer_handler(adapter);

	if (!is_drv_in_lps(adapter))
		rtw_dynamic_chk_wk_cmd(adapter);
}


#ifdef CONFIG_SET_SCAN_DENY_TIMER
inline bool rtw_is_scan_deny(_adapter *adapter)
{
	struct mlme_priv *mlmepriv = &adapter->mlmepriv;
	return (ATOMIC_READ(&mlmepriv->set_scan_deny) != 0) ? true : false;
}

inline void rtw_clear_scan_deny(_adapter *adapter)
{
	struct mlme_priv *mlmepriv = &adapter->mlmepriv;
	ATOMIC_SET(&mlmepriv->set_scan_deny, 0);
	if (0)
		RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapter));
}

void rtw_set_scan_deny_timer_hdl(_adapter *adapter)
{
	rtw_clear_scan_deny(adapter);
}
void rtw_set_scan_deny(_adapter *adapter, u32 ms)
{
	struct mlme_priv *mlmepriv = &adapter->mlmepriv;
	if (0)
		RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapter));
	ATOMIC_SET(&mlmepriv->set_scan_deny, 1);
	_set_timer(&mlmepriv->set_scan_deny_timer, ms);
}
#endif

#ifdef CONFIG_LAYER2_ROAMING
/*
* Select a new roaming candidate from the original @param candidate and @param competitor
* @return true: candidate is updated
* @return false: candidate is not updated
*/
static int rtw_check_roaming_candidate(struct mlme_priv *mlme
	, struct wlan_network **candidate, struct wlan_network *competitor)
{
	int updated = false;
	_adapter *adapter = container_of(mlme, _adapter, mlmepriv);
#ifdef CONFIG_RTW_80211R
	ft_priv *pftpriv = &mlme->ftpriv;
	u32 mdie_len = 0;
	u8 *ptmp = NULL;
#endif


	if (is_same_ess(&competitor->network, &mlme->cur_network.network) == false)
		goto exit;

	if (rtw_is_desired_network(adapter, competitor) == false)
		goto exit;

#ifdef CONFIG_LAYER2_ROAMING
	if (mlme->need_to_roam == false)
		goto exit;
#endif

#ifdef CONFIG_RTW_80211R
	if (rtw_chk_ft_flags(adapter, RTW_FT_SUPPORTED)) {
		ptmp = rtw_get_ie(&competitor->network.IEs[12], _MDIE_, &mdie_len, competitor->network.IELength-12);
		if (ptmp) {
			if (memcmp(&pftpriv->mdid, ptmp+2, 2))
				goto exit;

			/*The candidate don't support over-the-DS*/
			if (rtw_chk_ft_flags(adapter, RTW_FT_STA_OVER_DS_SUPPORTED)) {
				if ((rtw_chk_ft_flags(adapter, RTW_FT_OVER_DS_SUPPORTED) && !(*(ptmp+4) & 0x01)) ||
					(!rtw_chk_ft_flags(adapter, RTW_FT_OVER_DS_SUPPORTED) && (*(ptmp+4) & 0x01))) {
					RTW_INFO("FT: ignore the candidate(" MAC_FMT ") for over-the-DS\n", MAC_ARG(competitor->network.MacAddress));
					rtw_clr_ft_flags(adapter, RTW_FT_OVER_DS_SUPPORTED);
					goto exit;
				}
			}
		} else
			goto exit;
	}
#endif

	RTW_INFO("roam candidate:%s %s("MAC_FMT", ch%3u) rssi:%d, age:%5d\n",
		 (competitor == mlme->cur_network_scanned) ? "*" : " " ,
		 competitor->network.Ssid.Ssid,
		 MAC_ARG(competitor->network.MacAddress),
		 competitor->network.Configuration.DSConfig,
		 (int)competitor->network.Rssi,
		 rtw_get_passing_time_ms(competitor->last_scanned)
		);

	/* got specific addr to roam */
	if (!is_zero_mac_addr(mlme->roam_tgt_addr)) {
		if (!memcmp(mlme->roam_tgt_addr, competitor->network.MacAddress, ETH_ALEN))
			goto update;
		else
			goto exit;
	}
	if (rtw_get_passing_time_ms((u32)competitor->last_scanned) >= mlme->roam_scanr_exp_ms)
		goto exit;

	if (competitor->network.Rssi - mlme->cur_network_scanned->network.Rssi < mlme->roam_rssi_diff_th)
		goto exit;

	if (*candidate != NULL && (*candidate)->network.Rssi >= competitor->network.Rssi)
		goto exit;
update:
	*candidate = competitor;
	updated = true;

exit:
	return updated;
}

int rtw_select_roaming_candidate(struct mlme_priv *mlme)
{
	unsigned long	irqL;
	int ret = _FAIL;
	_list	*phead;
	_adapter *adapter;
	_queue	*queue	= &(mlme->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	struct	wlan_network	*candidate = NULL;
	u8		bSupportAntDiv = false;


	if (mlme->cur_network_scanned == NULL) {
		rtw_warn_on(1);
		return ret;
	}

	_enter_critical_bh(&(mlme->scanned_queue.lock), &irqL);
	phead = get_list_head(queue);
	adapter = (_adapter *)mlme->nic_hdl;

	mlme->pscanned = get_next(phead);

	while (!rtw_end_of_queue_search(phead, mlme->pscanned)) {

		pnetwork = LIST_CONTAINOR(mlme->pscanned, struct wlan_network, list);
		if (pnetwork == NULL) {
			ret = _FAIL;
			goto exit;
		}

		mlme->pscanned = get_next(mlme->pscanned);

		if (0)
			RTW_INFO("%s("MAC_FMT", ch%u) rssi:%d\n"
				 , pnetwork->network.Ssid.Ssid
				 , MAC_ARG(pnetwork->network.MacAddress)
				 , pnetwork->network.Configuration.DSConfig
				 , (int)pnetwork->network.Rssi);

		rtw_check_roaming_candidate(mlme, &candidate, pnetwork);

	}

	if (candidate == NULL) {
		RTW_INFO("%s: return _FAIL(candidate == NULL)\n", __func__);
		ret = _FAIL;
		goto exit;
	} else {
		RTW_INFO("%s: candidate: %s("MAC_FMT", ch:%u)\n", __func__,
			candidate->network.Ssid.Ssid, MAC_ARG(candidate->network.MacAddress),
			 candidate->network.Configuration.DSConfig);

		mlme->roam_network = candidate;

		if (!memcmp(candidate->network.MacAddress, mlme->roam_tgt_addr, ETH_ALEN))
			memset(mlme->roam_tgt_addr, 0, ETH_ALEN);
	}

	ret = _SUCCESS;
exit:
	_exit_critical_bh(&(mlme->scanned_queue.lock), &irqL);

	return ret;
}
#endif /* CONFIG_LAYER2_ROAMING */

/*
* Select a new join candidate from the original @param candidate and @param competitor
* @return true: candidate is updated
* @return false: candidate is not updated
*/
static int rtw_check_join_candidate(struct mlme_priv *mlme
	    , struct wlan_network **candidate, struct wlan_network *competitor)
{
	int updated = false;
	_adapter *adapter = container_of(mlme, _adapter, mlmepriv);


	/* check bssid, if needed */
	if (mlme->assoc_by_bssid) {
		if (!memcmp(competitor->network.MacAddress, mlme->assoc_bssid, ETH_ALEN) == false)
			goto exit;
	}

	/* check ssid, if needed */
	if (mlme->assoc_ssid.Ssid[0] && mlme->assoc_ssid.SsidLength) {
		if (competitor->network.Ssid.SsidLength != mlme->assoc_ssid.SsidLength
		    || !memcmp(competitor->network.Ssid.Ssid, mlme->assoc_ssid.Ssid, mlme->assoc_ssid.SsidLength) == false
		   )
			goto exit;
	}

	if (rtw_is_desired_network(adapter, competitor)  == false)
		goto exit;

#ifdef CONFIG_LAYER2_ROAMING
	if (rtw_to_roam(adapter) > 0) {
		if (rtw_get_passing_time_ms((u32)competitor->last_scanned) >= mlme->roam_scanr_exp_ms
		    || is_same_ess(&competitor->network, &mlme->cur_network.network) == false
		   )
			goto exit;
	}
#endif

	if (*candidate == NULL || (*candidate)->network.Rssi < competitor->network.Rssi) {
		*candidate = competitor;
		updated = true;
	}

	if (updated) {
		RTW_INFO("[by_bssid:%u][assoc_ssid:%s][to_roam:%u] "
			 "new candidate: %s("MAC_FMT", ch%u) rssi:%d\n",
			 mlme->assoc_by_bssid,
			 mlme->assoc_ssid.Ssid,
			 rtw_to_roam(adapter),
			 (*candidate)->network.Ssid.Ssid,
			 MAC_ARG((*candidate)->network.MacAddress),
			 (*candidate)->network.Configuration.DSConfig,
			 (int)(*candidate)->network.Rssi
			);
	}

exit:
	return updated;
}

/*
Calling context:
The caller of the sub-routine will be in critical section...

The caller must hold the following spinlock

pmlmepriv->lock


*/

int rtw_select_and_join_from_scanned_queue(struct mlme_priv *pmlmepriv)
{
	unsigned long	irqL;
	int ret;
	_list	*phead;
	_adapter *adapter;
	_queue	*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	struct	wlan_network	*candidate = NULL;
	u8		bSupportAntDiv = false;


	adapter = (_adapter *)pmlmepriv->nic_hdl;

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

#ifdef CONFIG_LAYER2_ROAMING
	if (pmlmepriv->roam_network) {
		candidate = pmlmepriv->roam_network;
		pmlmepriv->roam_network = NULL;
		goto candidate_exist;
	}
#endif

	phead = get_list_head(queue);
	pmlmepriv->pscanned = get_next(phead);

	while (!rtw_end_of_queue_search(phead, pmlmepriv->pscanned)) {

		pnetwork = LIST_CONTAINOR(pmlmepriv->pscanned, struct wlan_network, list);
		if (pnetwork == NULL) {
			ret = _FAIL;
			goto exit;
		}

		pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

		if (0)
			RTW_INFO("%s("MAC_FMT", ch%u) rssi:%d\n"
				 , pnetwork->network.Ssid.Ssid
				 , MAC_ARG(pnetwork->network.MacAddress)
				 , pnetwork->network.Configuration.DSConfig
				 , (int)pnetwork->network.Rssi);

		rtw_check_join_candidate(pmlmepriv, &candidate, pnetwork);

	}

	if (candidate == NULL) {
		RTW_INFO("%s: return _FAIL(candidate == NULL)\n", __func__);
#ifdef CONFIG_WOWLAN
		_clr_fwstate_(pmlmepriv, _FW_LINKED | _FW_UNDER_LINKING);
#endif
		ret = _FAIL;
		goto exit;
	} else {
		RTW_INFO("%s: candidate: %s("MAC_FMT", ch:%u)\n", __func__,
			candidate->network.Ssid.Ssid, MAC_ARG(candidate->network.MacAddress),
			 candidate->network.Configuration.DSConfig);
		goto candidate_exist;
	}

candidate_exist:

	/* check for situation of  _FW_LINKED */
	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
		RTW_INFO("%s: _FW_LINKED while ask_for_joinbss!!!\n", __func__);

		rtw_disassoc_cmd(adapter, 0, true);
		rtw_indicate_disconnect(adapter, 0, false);
		rtw_free_assoc_resources(adapter, 0);
	}

#ifdef CONFIG_ANTENNA_DIVERSITY
	rtw_hal_get_def_var(adapter, HAL_DEF_IS_SUPPORT_ANT_DIV, &(bSupportAntDiv));
	if (bSupportAntDiv) {
		u8 CurrentAntenna;
		rtw_hal_get_odm_var(adapter, HAL_ODM_ANTDIV_SELECT, &(CurrentAntenna), NULL);
		RTW_INFO("#### Opt_Ant_(%s) , cur_Ant(%s)\n",
			(MAIN_ANT == candidate->network.PhyInfo.Optimum_antenna) ? "MAIN_ANT" : "AUX_ANT",
			 (MAIN_ANT == CurrentAntenna) ? "MAIN_ANT" : "AUX_ANT"
			);
	}
#endif
	set_fwstate(pmlmepriv, _FW_UNDER_LINKING);
	ret = rtw_joinbss_cmd(adapter, candidate);

exit:
	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);


	return ret;
}

sint rtw_set_auth(_adapter *adapter, struct security_priv *psecuritypriv)
{
	struct	cmd_obj *pcmd;
	struct	setauth_parm *psetauthparm;
	struct	cmd_priv	*pcmdpriv = &(adapter->cmdpriv);
	sint		res = _SUCCESS;


	pcmd = (struct	cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
	if (pcmd == NULL) {
		res = _FAIL; /* try again */
		goto exit;
	}

	psetauthparm = (struct setauth_parm *)rtw_zmalloc(sizeof(struct setauth_parm));
	if (psetauthparm == NULL) {
		rtw_mfree((unsigned char *)pcmd, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	memset(psetauthparm, 0, sizeof(struct setauth_parm));
	psetauthparm->mode = (unsigned char)psecuritypriv->dot11AuthAlgrthm;

	pcmd->cmdcode = _SetAuth_CMD_;
	pcmd->parmbuf = (unsigned char *)psetauthparm;
	pcmd->cmdsz = (sizeof(struct setauth_parm));
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;


	INIT_LIST_HEAD(&pcmd->list);


	res = rtw_enqueue_cmd(pcmdpriv, pcmd);

exit:


	return res;

}


sint rtw_set_key(_adapter *adapter, struct security_priv *psecuritypriv, sint keyid, u8 set_tx, bool enqueue)
{
	u8	keylen;
	struct cmd_obj		*pcmd;
	struct setkey_parm	*psetkeyparm;
	struct cmd_priv		*pcmdpriv = &(adapter->cmdpriv);
	struct mlme_priv		*pmlmepriv = &(adapter->mlmepriv);
	sint	res = _SUCCESS;


	psetkeyparm = (struct setkey_parm *)rtw_zmalloc(sizeof(struct setkey_parm));
	if (psetkeyparm == NULL) {
		res = _FAIL;
		goto exit;
	}
	memset(psetkeyparm, 0, sizeof(struct setkey_parm));

	if (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) {
		psetkeyparm->algorithm = (unsigned char)psecuritypriv->dot118021XGrpPrivacy;
	} else {
		psetkeyparm->algorithm = (u8)psecuritypriv->dot11PrivacyAlgrthm;

	}
	psetkeyparm->keyid = (u8)keyid;/* 0~3 */
	psetkeyparm->set_tx = set_tx;
	if (is_wep_enc(psetkeyparm->algorithm))
		adapter->securitypriv.key_mask |= BIT(psetkeyparm->keyid);

	RTW_INFO("==> rtw_set_key algorithm(%x),keyid(%x),key_mask(%x)\n", psetkeyparm->algorithm, psetkeyparm->keyid, adapter->securitypriv.key_mask);

	switch (psetkeyparm->algorithm) {

	case _WEP40_:
		keylen = 5;
		memcpy(&(psetkeyparm->key[0]), &(psecuritypriv->dot11DefKey[keyid].skey[0]), keylen);
		break;
	case _WEP104_:
		keylen = 13;
		memcpy(&(psetkeyparm->key[0]), &(psecuritypriv->dot11DefKey[keyid].skey[0]), keylen);
		break;
	case _TKIP_:
		keylen = 16;
		memcpy(&psetkeyparm->key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		psetkeyparm->grpkey = 1;
		break;
	case _AES_:
		keylen = 16;
		memcpy(&psetkeyparm->key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		psetkeyparm->grpkey = 1;
		break;
	default:
		res = _FAIL;
		rtw_mfree((unsigned char *)psetkeyparm, sizeof(struct setkey_parm));
		goto exit;
	}


	if (enqueue) {
		pcmd = (struct	cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
		if (pcmd == NULL) {
			rtw_mfree((unsigned char *)psetkeyparm, sizeof(struct setkey_parm));
			res = _FAIL; /* try again */
			goto exit;
		}

		pcmd->cmdcode = _SetKey_CMD_;
		pcmd->parmbuf = (u8 *)psetkeyparm;
		pcmd->cmdsz = (sizeof(struct setkey_parm));
		pcmd->rsp = NULL;
		pcmd->rspsz = 0;

		INIT_LIST_HEAD(&pcmd->list);

		/* sema_init(&(pcmd->cmd_sem), 0); */

		res = rtw_enqueue_cmd(pcmdpriv, pcmd);
	} else {
		setkey_hdl(adapter, (u8 *)psetkeyparm);
		rtw_mfree((u8 *) psetkeyparm, sizeof(struct setkey_parm));
	}
exit:
	return res;

}


/* adjust IEs for rtw_joinbss_cmd in WMM */
int rtw_restruct_wmm_ie(_adapter *adapter, u8 *in_ie, u8 *out_ie, uint in_len, uint initial_out_len)
{
	unsigned	int ielength = 0;
	unsigned int i, j;

	i = 12; /* after the fixed IE */
	while (i < in_len) {
		ielength = initial_out_len;

		if (in_ie[i] == 0xDD && in_ie[i + 2] == 0x00 && in_ie[i + 3] == 0x50  && in_ie[i + 4] == 0xF2 && in_ie[i + 5] == 0x02 && i + 5 < in_len) { /* WMM element ID and OUI */
			/* Append WMM IE to the last index of out_ie */
			for (j = i; j < i + 9; j++) {
				out_ie[ielength] = in_ie[j];
				ielength++;
			}
			out_ie[initial_out_len + 1] = 0x07;
			out_ie[initial_out_len + 6] = 0x00;
			out_ie[initial_out_len + 8] = 0x00;

			break;
		}

		i += (in_ie[i + 1] + 2); /* to the next IE element */
	}

	return ielength;

}


/*
 * Ported from 8185: IsInPreAuthKeyList(). (Renamed from SecIsInPreAuthKeyList(), 2006-10-13.)
 * Added by Annie, 2006-05-07.
 *
 * Search by BSSID,
 * Return Value:
 *		-1		:if there is no pre-auth key in the  table
 *		>=0		:if there is pre-auth key, and   return the entry id
 *
 *   */

static int SecIsInPMKIDList(_adapter *Adapter, u8 *bssid)
{
	struct security_priv *psecuritypriv = &Adapter->securitypriv;
	int i = 0;

	do {
		if ((psecuritypriv->PMKIDList[i].bUsed) &&
		    (!memcmp(psecuritypriv->PMKIDList[i].Bssid, bssid, ETH_ALEN)))
			break;
		else {
			i++;
			/* continue; */
		}

	} while (i < NUM_PMKID_CACHE);

	if (i == NUM_PMKID_CACHE) {
		i = -1;/* Could not find. */
	} else {
		/* There is one Pre-Authentication Key for the specific BSSID. */
	}

	return i;

}

/*
 * Check the RSN IE length
 * If the RSN IE length <= 20, the RSN IE didn't include the PMKID information
 * 0-11th element in the array are the fixed IE
 * 12th element in the array is the IE
 * 13th element in the array is the IE length
 *   */

static int rtw_append_pmkid(_adapter *adapter, int iEntry, u8 *ie, uint ie_len)
{
	struct security_priv *sec = &adapter->securitypriv;

	if (ie[13] > 20) {
		int i;
		u16 pmkid_cnt = RTW_GET_LE16(ie + 14 + 20);
		if (pmkid_cnt == 1 && !memcmp(ie + 14 + 20 + 2, &sec->PMKIDList[iEntry].PMKID, 16)) {
			RTW_INFO(FUNC_ADPT_FMT" has carried the same PMKID:"KEY_FMT"\n"
				, FUNC_ADPT_ARG(adapter), KEY_ARG(&sec->PMKIDList[iEntry].PMKID));
			goto exit;
		}

		RTW_INFO(FUNC_ADPT_FMT" remove original PMKID, count:%u\n"
			 , FUNC_ADPT_ARG(adapter), pmkid_cnt);

		for (i = 0; i < pmkid_cnt; i++)
			RTW_INFO("    "KEY_FMT"\n", KEY_ARG(ie + 14 + 20 + 2 + i * 16));

		ie_len -= 2 + pmkid_cnt * 16;
		ie[13] = 20;
	}

	if (ie[13] <= 20) {
		/* The RSN IE didn't include the PMK ID, append the PMK information */

		RTW_INFO(FUNC_ADPT_FMT" append PMKID:"KEY_FMT"\n"
			, FUNC_ADPT_ARG(adapter), KEY_ARG(&sec->PMKIDList[iEntry].PMKID));

		RTW_PUT_LE16(&ie[ie_len], 1);
		ie_len += 2;

		memcpy(&ie[ie_len], &sec->PMKIDList[iEntry].PMKID, 16);
		ie_len += 16;

		ie[13] += 18;/* PMKID length = 2+16 */
	}

exit:
	return ie_len;
}

static int rtw_remove_pmkid(_adapter *adapter, u8 *ie, uint ie_len)
{
	struct security_priv *sec = &adapter->securitypriv;
	int i;
	u16 pmkid_cnt = RTW_GET_LE16(ie + 14 + 20);

	if (ie[13] <= 20)
		goto exit;

	RTW_INFO(FUNC_ADPT_FMT" remove original PMKID, count:%u\n"
		 , FUNC_ADPT_ARG(adapter), pmkid_cnt);

	for (i = 0; i < pmkid_cnt; i++)
		RTW_INFO("    "KEY_FMT"\n", KEY_ARG(ie + 14 + 20 + 2 + i * 16));

	ie_len -= 2 + pmkid_cnt * 16;
	ie[13] = 20;

exit:
	return ie_len;
}

sint rtw_restruct_sec_ie(_adapter *adapter, u8 *in_ie, u8 *out_ie, uint in_len)
{
	u8 authmode = 0x0, securitytype, match;
	u8 sec_ie[255], uncst_oui[4], bkup_ie[255];
	u8 wpa_oui[4] = {0x0, 0x50, 0xf2, 0x01};
	uint	ielength, cnt, remove_cnt;
	int iEntry;

	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct security_priv *psecuritypriv = &adapter->securitypriv;
	uint	ndisauthmode = psecuritypriv->ndisauthtype;
	uint ndissecuritytype = psecuritypriv->ndisencryptstatus;



	/* copy fixed ie only */
	memcpy(out_ie, in_ie, 12);
	ielength = 12;
	if ((ndisauthmode == Ndis802_11AuthModeWPA) || (ndisauthmode == Ndis802_11AuthModeWPAPSK))
		authmode = _WPA_IE_ID_;
	if ((ndisauthmode == Ndis802_11AuthModeWPA2) || (ndisauthmode == Ndis802_11AuthModeWPA2PSK))
		authmode = _WPA2_IE_ID_;

	if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		memcpy(out_ie + ielength, psecuritypriv->wps_ie, psecuritypriv->wps_ie_len);

		ielength += psecuritypriv->wps_ie_len;
	} else if ((authmode == _WPA_IE_ID_) || (authmode == _WPA2_IE_ID_)) {
		/* copy RSN or SSN		 */
		memcpy(&out_ie[ielength], &psecuritypriv->supplicant_ie[0], psecuritypriv->supplicant_ie[1] + 2);
		/* debug for CONFIG_IEEE80211W
		{
			int jj;
			RTW_INFO("supplicant_ie_length=%d &&&&&&&&&&&&&&&&&&&\n", psecuritypriv->supplicant_ie[1]+2);
			for(jj=0; jj < psecuritypriv->supplicant_ie[1]+2; jj++)
				RTW_INFO(" %02x ", psecuritypriv->supplicant_ie[jj]);
			RTW_INFO("\n");
		}*/
		ielength += psecuritypriv->supplicant_ie[1] + 2;
		rtw_report_sec_ie(adapter, authmode, psecuritypriv->supplicant_ie);

#ifdef CONFIG_DRVEXT_MODULE
		drvext_report_sec_ie(&adapter->drvextpriv, authmode, sec_ie);
#endif
	}

	iEntry = SecIsInPMKIDList(adapter, pmlmepriv->assoc_bssid);
	if (iEntry < 0) {
		if (authmode == _WPA2_IE_ID_)
			ielength = rtw_remove_pmkid(adapter, out_ie, ielength);
	} else {
		if (authmode == _WPA2_IE_ID_)
			ielength = rtw_append_pmkid(adapter, iEntry, out_ie, ielength);
	}


	return ielength;
}

void rtw_init_registrypriv_dev_network(_adapter *adapter)
{
	struct registry_priv *pregistrypriv = &adapter->registrypriv;
	WLAN_BSSID_EX    *pdev_network = &pregistrypriv->dev_network;
	u8 *myhwaddr = adapter_mac_addr(adapter);


	memcpy(pdev_network->MacAddress, myhwaddr, ETH_ALEN);

	memcpy(&pdev_network->Ssid, &pregistrypriv->ssid, sizeof(NDIS_802_11_SSID));

	pdev_network->Configuration.Length = sizeof(NDIS_802_11_CONFIGURATION);
	pdev_network->Configuration.BeaconPeriod = 100;
	pdev_network->Configuration.FHConfig.Length = 0;
	pdev_network->Configuration.FHConfig.HopPattern = 0;
	pdev_network->Configuration.FHConfig.HopSet = 0;
	pdev_network->Configuration.FHConfig.DwellTime = 0;



}

void rtw_update_registrypriv_dev_network(_adapter *adapter)
{
	int sz = 0;
	struct registry_priv *pregistrypriv = &adapter->registrypriv;
	WLAN_BSSID_EX    *pdev_network = &pregistrypriv->dev_network;
	struct	security_priv	*psecuritypriv = &adapter->securitypriv;
	struct	wlan_network	*cur_network = &adapter->mlmepriv.cur_network;
	/* struct	xmit_priv	*pxmitpriv = &adapter->xmitpriv; */
	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;

	pdev_network->Privacy = (psecuritypriv->dot11PrivacyAlgrthm > 0 ? 1 : 0) ; /* adhoc no 802.1x */

	pdev_network->Rssi = 0;

	switch (pregistrypriv->wireless_mode) {
	case WIRELESS_11B:
		pdev_network->NetworkTypeInUse = (Ndis802_11DS);
		break;
	case WIRELESS_11G:
	case WIRELESS_11BG:
	case WIRELESS_11_24N:
	case WIRELESS_11G_24N:
	case WIRELESS_11BG_24N:
		pdev_network->NetworkTypeInUse = (Ndis802_11OFDM24);
		break;
	case WIRELESS_11A:
	case WIRELESS_11A_5N:
		pdev_network->NetworkTypeInUse = (Ndis802_11OFDM5);
		break;
	case WIRELESS_11ABGN:
		if (pregistrypriv->channel > 14)
			pdev_network->NetworkTypeInUse = (Ndis802_11OFDM5);
		else
			pdev_network->NetworkTypeInUse = (Ndis802_11OFDM24);
		break;
	default:
		/* TODO */
		break;
	}

	pdev_network->Configuration.DSConfig = (pregistrypriv->channel);

	if (cur_network->network.InfrastructureMode == Ndis802_11IBSS) {
		pdev_network->Configuration.ATIMWindow = (0);

		if (pmlmeext->cur_channel != 0)
			pdev_network->Configuration.DSConfig = pmlmeext->cur_channel;
		else
			pdev_network->Configuration.DSConfig = 1;
	}

	pdev_network->InfrastructureMode = (cur_network->network.InfrastructureMode);

	/* 1. Supported rates */
	/* 2. IE */

	/* rtw_set_supported_rate(pdev_network->SupportedRates, pregistrypriv->wireless_mode) ; */ /* will be called in rtw_generate_ie */
	sz = rtw_generate_ie(pregistrypriv);

	pdev_network->IELength = sz;

	pdev_network->Length = get_WLAN_BSSID_EX_sz((WLAN_BSSID_EX *)pdev_network);

	/* notes: translate IELength & Length after assign the Length to cmdsz in createbss_cmd(); */
	/* pdev_network->IELength = cpu_to_le32(sz); */


}

void rtw_get_encrypt_decrypt_from_registrypriv(_adapter *adapter)
{



}

/* the fucntion is at passive_level */
void rtw_joinbss_reset(_adapter *padapter)
{
	u8	threshold;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	/* todo: if you want to do something io/reg/hw setting before join_bss, please add code here */
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;

	pmlmepriv->num_FortyMHzIntolerant = 0;

	pmlmepriv->num_sta_no_ht = 0;

	phtpriv->ampdu_enable = false;/* reset to disabled */

	/* TH=1 => means that invalidate usb rx aggregation */
	/* TH=0 => means that validate usb rx aggregation, use init value. */
	if (phtpriv->ht_option) {
		if (padapter->registrypriv.wifi_spec == 1)
			threshold = 1;
		else
			threshold = 0;
		rtw_hal_set_hwreg(padapter, HW_VAR_RXDMA_AGG_PG_TH, (u8 *)(&threshold));
	} else {
		threshold = 1;
		rtw_hal_set_hwreg(padapter, HW_VAR_RXDMA_AGG_PG_TH, (u8 *)(&threshold));
	}
}

void	rtw_ht_use_default_setting(_adapter *padapter)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	struct registry_priv	*pregistrypriv = &padapter->registrypriv;
	bool		bHwLDPCSupport = false, bHwSTBCSupport = false;
#ifdef CONFIG_BEAMFORMING
	bool		bHwSupportBeamformer = false, bHwSupportBeamformee = false;
#endif /* CONFIG_BEAMFORMING */

	if (pregistrypriv->wifi_spec)
		phtpriv->bss_coexist = 1;
	else
		phtpriv->bss_coexist = 0;

	phtpriv->sgi_40m = TEST_FLAG(pregistrypriv->short_gi, BIT1) ? true : false;
	phtpriv->sgi_20m = TEST_FLAG(pregistrypriv->short_gi, BIT0) ? true : false;

	/* LDPC support */
	rtw_hal_get_def_var(padapter, HAL_DEF_RX_LDPC, (u8 *)&bHwLDPCSupport);
	CLEAR_FLAGS(phtpriv->ldpc_cap);
	if (bHwLDPCSupport) {
		if (TEST_FLAG(pregistrypriv->ldpc_cap, BIT4))
			SET_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_RX);
	}
	rtw_hal_get_def_var(padapter, HAL_DEF_TX_LDPC, (u8 *)&bHwLDPCSupport);
	if (bHwLDPCSupport) {
		if (TEST_FLAG(pregistrypriv->ldpc_cap, BIT5))
			SET_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_TX);
	}
	if (phtpriv->ldpc_cap)
		RTW_INFO("[HT] HAL Support LDPC = 0x%02X\n", phtpriv->ldpc_cap);

	/* STBC */
	rtw_hal_get_def_var(padapter, HAL_DEF_TX_STBC, (u8 *)&bHwSTBCSupport);
	CLEAR_FLAGS(phtpriv->stbc_cap);
	if (bHwSTBCSupport) {
		if (TEST_FLAG(pregistrypriv->stbc_cap, BIT5))
			SET_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX);
	}
	rtw_hal_get_def_var(padapter, HAL_DEF_RX_STBC, (u8 *)&bHwSTBCSupport);
	if (bHwSTBCSupport) {
		if (TEST_FLAG(pregistrypriv->stbc_cap, BIT4))
			SET_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_RX);
	}
	if (phtpriv->stbc_cap)
		RTW_INFO("[HT] HAL Support STBC = 0x%02X\n", phtpriv->stbc_cap);

	/* Beamforming setting */
	CLEAR_FLAGS(phtpriv->beamform_cap);
#ifdef CONFIG_BEAMFORMING
	rtw_hal_get_def_var(padapter, HAL_DEF_EXPLICIT_BEAMFORMER, (u8 *)&bHwSupportBeamformer);
	rtw_hal_get_def_var(padapter, HAL_DEF_EXPLICIT_BEAMFORMEE, (u8 *)&bHwSupportBeamformee);
	if (TEST_FLAG(pregistrypriv->beamform_cap, BIT4) && bHwSupportBeamformer) {
		SET_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMER_ENABLE);
		RTW_INFO("[HT] HAL Support Beamformer\n");
	}
	if (TEST_FLAG(pregistrypriv->beamform_cap, BIT5) && bHwSupportBeamformee) {
		SET_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMEE_ENABLE);
		RTW_INFO("[HT] HAL Support Beamformee\n");
	}
#endif /* CONFIG_BEAMFORMING */
}
void rtw_build_wmm_ie_ht(_adapter *padapter, u8 *out_ie, uint *pout_len)
{
	unsigned char WMM_IE[] = {0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00};
	int out_len;
	u8 *pframe;

	if (padapter->mlmepriv.qospriv.qos_option == 0) {
		out_len = *pout_len;
		pframe = rtw_set_ie(out_ie + out_len, _VENDOR_SPECIFIC_IE_,
				    _WMM_IE_Length_, WMM_IE, pout_len);

		padapter->mlmepriv.qospriv.qos_option = 1;
	}
}

/* the fucntion is >= passive_level */
unsigned int rtw_restructure_ht_ie(_adapter *padapter, u8 *in_ie, u8 *out_ie, uint in_len, uint *pout_len, u8 channel)
{
	u32 ielen, out_len;
	u32 rx_packet_offset, max_recvbuf_sz;
	HT_CAP_AMPDU_FACTOR max_rx_ampdu_factor;
	HT_CAP_AMPDU_DENSITY best_ampdu_density;
	unsigned char *p, *pframe;
	struct rtw_ieee80211_ht_cap ht_capie;
	u8	cbw40_enable = 0, rf_type = 0, operation_bw = 0, rf_num = 0, rx_stbc_nss = 0, rx_nss = 0;
	struct registry_priv *pregistrypriv = &padapter->registrypriv;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(padapter);

	phtpriv->ht_option = false;

	out_len = *pout_len;

	memset(&ht_capie, 0, sizeof(struct rtw_ieee80211_ht_cap));

	ht_capie.cap_info = cpu_to_le16(IEEE80211_HT_CAP_DSSSCCK40);

	if (phtpriv->sgi_20m)
		ht_capie.cap_info |= cpu_to_le16(IEEE80211_HT_CAP_SGI_20);

	/* Get HT BW */
	if (in_ie == NULL) {
		/* TDLS: TODO 20/40 issue */
		if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
			operation_bw = padapter->mlmeextpriv.cur_bwmode;
			if (operation_bw > CHANNEL_WIDTH_40)
				operation_bw = CHANNEL_WIDTH_40;
		} else
			/* TDLS: TODO 40? */
			operation_bw = CHANNEL_WIDTH_40;
	} else {
		p = rtw_get_ie(in_ie, _HT_ADD_INFO_IE_, &ielen, in_len);
		if (p && (ielen == sizeof(struct ieee80211_ht_addt_info))) {
			struct HT_info_element *pht_info = (struct HT_info_element *)(p + 2);
			if (pht_info->infos[0] & BIT(2)) {
				switch (pht_info->infos[0] & 0x3) {
				case 1:
				case 3:
					operation_bw = CHANNEL_WIDTH_40;
					break;
				default:
					operation_bw = CHANNEL_WIDTH_20;
					break;
				}
			} else
				operation_bw = CHANNEL_WIDTH_20;
		}
	}

	/* to disable 40M Hz support while gd_bw_40MHz_en = 0 */
	if (hal_chk_bw_cap(padapter, BW_CAP_40M)) {
		if (channel > 14) {
			if (REGSTY_IS_BW_5G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		} else {
			if (REGSTY_IS_BW_2G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		}
	}

	if ((cbw40_enable == 1) && (operation_bw == CHANNEL_WIDTH_40)) {
		ht_capie.cap_info |= cpu_to_le16(IEEE80211_HT_CAP_SUP_WIDTH);
		if (phtpriv->sgi_40m)
			ht_capie.cap_info |= cpu_to_le16(IEEE80211_HT_CAP_SGI_40);
	}

	/* todo: disable SM power save mode */
	ht_capie.cap_info |= cpu_to_le16(IEEE80211_HT_CAP_SM_PS);

	/* RX LDPC */
	if (TEST_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_RX)) {
		ht_capie.cap_info |= cpu_to_le16(IEEE80211_HT_CAP_LDPC_CODING);
		RTW_INFO("[HT] Declare supporting RX LDPC\n");
	}

	/* TX STBC */
	if (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX)) {
		ht_capie.cap_info |= cpu_to_le16(IEEE80211_HT_CAP_TX_STBC);
		RTW_INFO("[HT] Declare supporting TX STBC\n");
	}

	/* RX STBC */
	if (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_RX)) {
		if ((pregistrypriv->rx_stbc == 0x3) ||							/* enable for 2.4/5 GHz */
		    ((channel <= 14) && (pregistrypriv->rx_stbc == 0x1)) ||		/* enable for 2.4GHz */
		    ((channel > 14) && (pregistrypriv->rx_stbc == 0x2)) ||		/* enable for 5GHz */
		    (pregistrypriv->wifi_spec == 1)) {
			/* HAL_DEF_RX_STBC means STBC RX spatial stream, todo: VHT 4 streams */
			rtw_hal_get_def_var(padapter, HAL_DEF_RX_STBC, (u8 *)(&rx_stbc_nss));
			SET_HT_CAP_ELE_RX_STBC(&ht_capie, rx_stbc_nss);
			RTW_INFO("[HT] Declare supporting RX STBC = %d\n", rx_stbc_nss);
		}
	}

	/* fill default supported_mcs_set */
	memcpy(ht_capie.supp_mcs_set, pmlmeext->default_supported_mcs_set, 16);

	/* update default supported_mcs_set */
	rtw_hal_get_hwreg(padapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	rx_nss = rtw_min(rf_type_to_rf_rx_cnt(rf_type), hal_spec->rx_nss_num);

	switch (rx_nss) {
	case 1:
		set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_1R);
		break;
	case 2:
		#ifdef CONFIG_DISABLE_MCS13TO15
		if (((cbw40_enable == 1) && (operation_bw == CHANNEL_WIDTH_40)) && (pregistrypriv->wifi_spec != 1))
			set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_2R_13TO15_OFF);
		else
		#endif
			set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_2R);
		break;
	case 3:
		set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_3R);
		break;
	case 4:
		set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_4R);
		break;
	default:
		RTW_WARN("rf_type:%d or rx_nss:%u is not expected\n", rf_type, hal_spec->rx_nss_num);
	}

	rtw_hal_get_def_var(padapter, HAL_DEF_RX_PACKET_OFFSET, &rx_packet_offset);
	rtw_hal_get_def_var(padapter, HAL_DEF_MAX_RECVBUF_SZ, &max_recvbuf_sz);
	if (max_recvbuf_sz - rx_packet_offset >= (8191 - 256)) {
		RTW_INFO("%s IEEE80211_HT_CAP_MAX_AMSDU is set\n", __func__);
		ht_capie.cap_info = ht_capie.cap_info | cpu_to_le16(IEEE80211_HT_CAP_MAX_AMSDU);
	}
	if (padapter->driver_rx_ampdu_factor != 0xFF)
		max_rx_ampdu_factor = (HT_CAP_AMPDU_FACTOR)padapter->driver_rx_ampdu_factor;
	else
		rtw_hal_get_def_var(padapter, HW_VAR_MAX_RX_AMPDU_FACTOR, &max_rx_ampdu_factor);

	/* rtw_hal_get_def_var(padapter, HW_VAR_MAX_RX_AMPDU_FACTOR, &max_rx_ampdu_factor); */
	ht_capie.ampdu_params_info = (max_rx_ampdu_factor & 0x03);

	if (padapter->driver_rx_ampdu_spacing != 0xFF)
		ht_capie.ampdu_params_info |= ((padapter->driver_rx_ampdu_spacing & 0x07) << 2);
	else {
		if (padapter->securitypriv.dot11PrivacyAlgrthm == _AES_) {
			/*
			*	Todo : Each chip must to ask DD , this chip best ampdu_density setting
			*	By yiwei.sun
			*/
			rtw_hal_get_def_var(padapter, HW_VAR_BEST_AMPDU_DENSITY, &best_ampdu_density);

			ht_capie.ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_DENSITY & (best_ampdu_density << 2));

		} else
			ht_capie.ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_DENSITY & 0x00);
	}
#ifdef CONFIG_BEAMFORMING
	ht_capie.tx_BF_cap_info = 0;

	/* HT Beamformer*/
	if (TEST_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMER_ENABLE)) {
		/* Transmit NDP Capable */
		SET_HT_CAP_TXBF_TRANSMIT_NDP_CAP(&ht_capie, 1);
		/* Explicit Compressed Steering Capable */
		SET_HT_CAP_TXBF_EXPLICIT_COMP_STEERING_CAP(&ht_capie, 1);
		/* Compressed Steering Number Antennas */
		SET_HT_CAP_TXBF_COMP_STEERING_NUM_ANTENNAS(&ht_capie, 1);
		rtw_hal_get_def_var(padapter, HAL_DEF_BEAMFORMER_CAP, (u8 *)&rf_num);
		SET_HT_CAP_TXBF_CHNL_ESTIMATION_NUM_ANTENNAS(&ht_capie, rf_num);
	}

	/* HT Beamformee */
	if (TEST_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMEE_ENABLE)) {
		/* Receive NDP Capable */
		SET_HT_CAP_TXBF_RECEIVE_NDP_CAP(&ht_capie, 1);
		/* Explicit Compressed Beamforming Feedback Capable */
		SET_HT_CAP_TXBF_EXPLICIT_COMP_FEEDBACK_CAP(&ht_capie, 2);
		rtw_hal_get_def_var(padapter, HAL_DEF_BEAMFORMEE_CAP, (u8 *)&rf_num);
		SET_HT_CAP_TXBF_COMP_STEERING_NUM_ANTENNAS(&ht_capie, rf_num);
	}
#endif/*CONFIG_BEAMFORMING*/

	pframe = rtw_set_ie(out_ie + out_len, _HT_CAPABILITY_IE_,
		sizeof(struct rtw_ieee80211_ht_cap), (unsigned char *)&ht_capie, pout_len);

	phtpriv->ht_option = true;

	if (in_ie != NULL) {
		p = rtw_get_ie(in_ie, _HT_ADD_INFO_IE_, &ielen, in_len);
		if (p && (ielen == sizeof(struct ieee80211_ht_addt_info))) {
			out_len = *pout_len;
			pframe = rtw_set_ie(out_ie + out_len, _HT_ADD_INFO_IE_, ielen, p + 2 , pout_len);
		}
	}

	return phtpriv->ht_option;

}

/* the fucntion is > passive_level (in critical_section) */
void rtw_update_ht_cap(_adapter *padapter, u8 *pie, uint ie_len, u8 channel)
{
	u8 *p, max_ampdu_sz;
	int len;
	/* struct sta_info *bmc_sta, *psta; */
	struct rtw_ieee80211_ht_cap *pht_capie;
	struct ieee80211_ht_addt_info *pht_addtinfo;
	/* struct recv_reorder_ctrl *preorder_ctrl; */
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	/* struct recv_priv *precvpriv = &padapter->recvpriv; */
	struct registry_priv *pregistrypriv = &padapter->registrypriv;
	/* struct wlan_network *pcur_network = &(pmlmepriv->cur_network);; */
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8 cbw40_enable = 0;


	if (!phtpriv->ht_option)
		return;

	if ((!pmlmeinfo->HT_info_enable) || (!pmlmeinfo->HT_caps_enable))
		return;

	RTW_INFO("+rtw_update_ht_cap()\n");

	/* maybe needs check if ap supports rx ampdu. */
	if ((phtpriv->ampdu_enable == false) && (pregistrypriv->ampdu_enable == 1)) {
		if (pregistrypriv->wifi_spec == 1) {
			/* remove this part because testbed AP should disable RX AMPDU */
			/* phtpriv->ampdu_enable = false; */
			phtpriv->ampdu_enable = true;
		} else
			phtpriv->ampdu_enable = true;
	} 


	/* check Max Rx A-MPDU Size */
	len = 0;
	p = rtw_get_ie(pie + sizeof(NDIS_802_11_FIXED_IEs), _HT_CAPABILITY_IE_, &len, ie_len - sizeof(NDIS_802_11_FIXED_IEs));
	if (p && len > 0) {
		pht_capie = (struct rtw_ieee80211_ht_cap *)(p + 2);
		max_ampdu_sz = (pht_capie->ampdu_params_info & IEEE80211_HT_CAP_AMPDU_FACTOR);
		max_ampdu_sz = 1 << (max_ampdu_sz + 3); /* max_ampdu_sz (kbytes); */

		/* RTW_INFO("rtw_update_ht_cap(): max_ampdu_sz=%d\n", max_ampdu_sz); */
		phtpriv->rx_ampdu_maxlen = max_ampdu_sz;

	}


	len = 0;
	p = rtw_get_ie(pie + sizeof(NDIS_802_11_FIXED_IEs), _HT_ADD_INFO_IE_, &len, ie_len - sizeof(NDIS_802_11_FIXED_IEs));
	if (p && len > 0) {
		pht_addtinfo = (struct ieee80211_ht_addt_info *)(p + 2);
		/* todo: */
	}

	if (hal_chk_bw_cap(padapter, BW_CAP_40M)) {
		if (channel > 14) {
			if (REGSTY_IS_BW_5G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		} else {
			if (REGSTY_IS_BW_2G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		}
	}

	/* update cur_bwmode & cur_ch_offset */
	if ((cbw40_enable) &&
	    (le16_to_cpu(pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info) & BIT(1)) &&
	    (pmlmeinfo->HT_info.infos[0] & BIT(2))) {
		struct hal_spec_t *hal_spec = GET_HAL_SPEC(padapter);
		int i;
		u8	rf_type = RF_1T1R;
		u8 tx_nss = 0;

		rtw_hal_get_hwreg(padapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
		tx_nss = rtw_min(rf_type_to_rf_tx_cnt(rf_type), hal_spec->tx_nss_num);

		/* update the MCS set */
		for (i = 0; i < 16; i++)
			pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate[i] &= pmlmeext->default_supported_mcs_set[i];

		/* update the MCS rates */
		switch (tx_nss) {
		case 1:
			set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_1R);
			break;
		case 2:
			#ifdef CONFIG_DISABLE_MCS13TO15
			if (pmlmeext->cur_bwmode == CHANNEL_WIDTH_40 && pregistrypriv->wifi_spec != 1)
				set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_2R_13TO15_OFF);
			else
			#endif
				set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_2R);
			break;
		case 3:
			set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_3R);
			break;
		case 4:
			set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_4R);
			break;
		default:
			RTW_WARN("rf_type:%d or tx_nss_num:%u is not expected\n", rf_type, hal_spec->tx_nss_num);
		}

		/* switch to the 40M Hz mode accoring to the AP */
		/* pmlmeext->cur_bwmode = CHANNEL_WIDTH_40; */
		switch ((pmlmeinfo->HT_info.infos[0] & 0x3)) {
		case EXTCHNL_OFFSET_UPPER:
			pmlmeext->cur_ch_offset = HAL_PRIME_CHNL_OFFSET_LOWER;
			break;

		case EXTCHNL_OFFSET_LOWER:
			pmlmeext->cur_ch_offset = HAL_PRIME_CHNL_OFFSET_UPPER;
			break;

		default:
			pmlmeext->cur_ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
			break;
		}
	}

	/*  */
	/* Config SM Power Save setting */
	/*  */
	pmlmeinfo->SM_PS = (le16_to_cpu(pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info) & 0x0C) >> 2;
	if (pmlmeinfo->SM_PS == WLAN_HT_CAP_SM_PS_STATIC)
		RTW_INFO("%s(): WLAN_HT_CAP_SM_PS_STATIC\n", __func__);

	/* Config current HT Protection mode. */
	pmlmeinfo->HT_protection = pmlmeinfo->HT_info.infos[1] & 0x3;
}

#ifdef CONFIG_TDLS
void rtw_issue_addbareq_cmd_tdls(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct sta_info *ptdls_sta = NULL;
	u8 issued;
	int priority;
	struct ht_priv	*phtpriv;

	priority = pattrib->priority;

	if (pattrib->direct_link) {
		ptdls_sta = rtw_get_stainfo(&padapter->stapriv, pattrib->dst);
		if ((ptdls_sta != NULL) && (ptdls_sta->tdls_sta_state & TDLS_LINKED_STATE)) {
			phtpriv = &ptdls_sta->htpriv;

			if ((phtpriv->ht_option) && (phtpriv->ampdu_enable == true)) {
				issued = (phtpriv->agg_enable_bitmap >> priority) & 0x1;
				issued |= (phtpriv->candidate_tid_bitmap >> priority) & 0x1;

				if (0 == issued) {
					RTW_INFO("[%s], p=%d\n", __func__, priority);
					ptdls_sta->htpriv.candidate_tid_bitmap |= BIT((u8)priority);
					rtw_addbareq_cmd(padapter, (u8)priority, pattrib->dst);
				}
			}
		}
	}
}
#endif /* CONFIG_TDLS */

void rtw_issue_addbareq_cmd(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	u8 issued;
	int priority;
	struct sta_info *psta = NULL;
	struct ht_priv	*phtpriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	s32 bmcst = IS_MCAST(pattrib->ra);

	/* if(bmcst || (padapter->mlmepriv.LinkDetectInfo.bTxBusyTraffic == false)) */
	if (bmcst || (padapter->mlmepriv.LinkDetectInfo.NumTxOkInPeriod < 100))
		return;

	priority = pattrib->priority;

#ifdef CONFIG_TDLS
	rtw_issue_addbareq_cmd_tdls(padapter, pxmitframe);
#endif /* CONFIG_TDLS */

	psta = rtw_get_stainfo(&padapter->stapriv, pattrib->ra);
	if (pattrib->psta != psta) {
		RTW_INFO("%s, pattrib->psta(%p) != psta(%p)\n", __func__, pattrib->psta, psta);
		return;
	}

	if (psta == NULL) {
		RTW_INFO("%s, psta==NUL\n", __func__);
		return;
	}

	if (!(psta->state & _FW_LINKED)) {
		RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, psta->state);
		return;
	}


	phtpriv = &psta->htpriv;

	if ((phtpriv->ht_option) && (phtpriv->ampdu_enable == true)) {
		issued = (phtpriv->agg_enable_bitmap >> priority) & 0x1;
		issued |= (phtpriv->candidate_tid_bitmap >> priority) & 0x1;

		if (0 == issued) {
			RTW_INFO("rtw_issue_addbareq_cmd, p=%d\n", priority);
			psta->htpriv.candidate_tid_bitmap |= BIT((u8)priority);
			rtw_addbareq_cmd(padapter, (u8) priority, pattrib->ra);
		}
	}

}

void rtw_append_exented_cap(_adapter *padapter, u8 *out_ie, uint *pout_len)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	u8	cap_content[8] = { 0 };
	u8	*pframe;
	u8   null_content[8] = {0};

	if (phtpriv->bss_coexist)
		SET_EXT_CAPABILITY_ELE_BSS_COEXIST(cap_content, 1);
	/*
		From 802.11 specification,if a STA does not support any of capabilities defined
		in the Extended Capabilities element, then the STA is not required to
		transmit the Extended Capabilities element.
	*/
	if (false == !memcmp(cap_content, null_content, 8))
		pframe = rtw_set_ie(out_ie + *pout_len, EID_EXTCapability, 8, cap_content , pout_len);
}

#ifdef CONFIG_LAYER2_ROAMING
inline void rtw_set_to_roam(_adapter *adapter, u8 to_roam)
{
	if (to_roam == 0)
		adapter->mlmepriv.to_join = false;
	adapter->mlmepriv.to_roam = to_roam;
}

inline u8 rtw_dec_to_roam(_adapter *adapter)
{
	adapter->mlmepriv.to_roam--;
	return adapter->mlmepriv.to_roam;
}

inline u8 rtw_to_roam(_adapter *adapter)
{
	return adapter->mlmepriv.to_roam;
}

void rtw_roaming(_adapter *padapter, struct wlan_network *tgt_network)
{
	unsigned long irqL;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	_rtw_roaming(padapter, tgt_network);
	_exit_critical_bh(&pmlmepriv->lock, &irqL);
}
void _rtw_roaming(_adapter *padapter, struct wlan_network *tgt_network)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct wlan_network *cur_network = &pmlmepriv->cur_network;
	int do_join_r;

	if (0 < rtw_to_roam(padapter)) {
		RTW_INFO("roaming from %s("MAC_FMT"), length:%d\n",
			cur_network->network.Ssid.Ssid, MAC_ARG(cur_network->network.MacAddress),
			 cur_network->network.Ssid.SsidLength);
		memcpy(&pmlmepriv->assoc_ssid, &cur_network->network.Ssid, sizeof(NDIS_802_11_SSID));

		pmlmepriv->assoc_by_bssid = false;

#ifdef CONFIG_WAPI_SUPPORT
		rtw_wapi_return_all_sta_info(padapter);
#endif

		while (1) {
			do_join_r = rtw_do_join(padapter);
			if (_SUCCESS == do_join_r)
				break;
			else {
				RTW_INFO("roaming do_join return %d\n", do_join_r);
				rtw_dec_to_roam(padapter);

				if (rtw_to_roam(padapter) > 0)
					continue;
				else {
					RTW_INFO("%s(%d) -to roaming fail, indicate_disconnect\n", __func__, __LINE__);
#ifdef CONFIG_RTW_80211R
					rtw_clr_ft_flags(padapter, RTW_FT_SUPPORTED|RTW_FT_OVER_DS_SUPPORTED);
					rtw_reset_ft_status(padapter);
#endif
					rtw_indicate_disconnect(padapter, 0, false);
					break;
				}
			}
		}
	}

}
#endif /* CONFIG_LAYER2_ROAMING */

bool rtw_adjust_chbw(_adapter *adapter, u8 req_ch, u8 *req_bw, u8 *req_offset)
{
	struct registry_priv *regsty = adapter_to_regsty(adapter);
	u8 allowed_bw;

	if (req_ch <= 14)
		allowed_bw = REGSTY_BW_2G(regsty);
	else
		allowed_bw = REGSTY_BW_5G(regsty);

	allowed_bw = hal_largest_bw(adapter, allowed_bw);

	if (allowed_bw == CHANNEL_WIDTH_80 && *req_bw > CHANNEL_WIDTH_80)
		*req_bw = CHANNEL_WIDTH_80;
	else if (allowed_bw == CHANNEL_WIDTH_40 && *req_bw > CHANNEL_WIDTH_40)
		*req_bw = CHANNEL_WIDTH_40;
	else if (allowed_bw == CHANNEL_WIDTH_20 && *req_bw > CHANNEL_WIDTH_20) {
		*req_bw = CHANNEL_WIDTH_20;
		*req_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
	} else
		return false;

	return true;
}

sint rtw_linked_check(_adapter *padapter)
{
	if ((check_fwstate(&padapter->mlmepriv, WIFI_AP_STATE)) ||
	    (check_fwstate(&padapter->mlmepriv, WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE))) {
		if (padapter->stapriv.asoc_sta_count > 2)
			return true;
	} else {
		/* Station mode */
		if (check_fwstate(&padapter->mlmepriv, _FW_LINKED))
			return true;
	}
	return false;
}
u8 rtw_is_adapter_up(_adapter *padapter)
{
	if (padapter == NULL)
		return false;

	if (RTW_CANNOT_RUN(padapter)) {
		RTW_INFO(FUNC_ADPT_FMT "-(bSurpriseRemoved) || ( bDriverStopped == true)\n", FUNC_ADPT_ARG(padapter));
		return false;
	}

	if (!rtw_is_hw_init_completed(padapter)) {
		/*RTW_INFO(FUNC_ADPT_FMT "-(hw_init_completed == false)\n", FUNC_ADPT_ARG(padapter));*/
		return false;
	}

	if (padapter->bup == false) {
		/*RTW_INFO(FUNC_ADPT_FMT "-(bup == false)\n", FUNC_ADPT_ARG(padapter));*/
		return false;
	}

	return true;
}

bool is_miracast_enabled(_adapter *adapter)
{
	bool enabled = 0;
#ifdef CONFIG_WFD
	struct wifi_display_info *wfdinfo = &adapter->wfd_info;

	enabled = (wfdinfo->stack_wfd_mode & (MIRACAST_SOURCE | MIRACAST_SINK))
		  || (wfdinfo->op_wfd_mode & (MIRACAST_SOURCE | MIRACAST_SINK));
#endif

	return enabled;
}

bool rtw_chk_miracast_mode(_adapter *adapter, u8 mode)
{
	bool ret = 0;
#ifdef CONFIG_WFD
	struct wifi_display_info *wfdinfo = &adapter->wfd_info;

	ret = (wfdinfo->stack_wfd_mode & mode) || (wfdinfo->op_wfd_mode & mode);
#endif

	return ret;
}

const char *get_miracast_mode_str(int mode)
{
	if (mode == MIRACAST_SOURCE)
		return "SOURCE";
	else if (mode == MIRACAST_SINK)
		return "SINK";
	else if (mode == (MIRACAST_SOURCE | MIRACAST_SINK))
		return "SOURCE&SINK";
	else if (mode == MIRACAST_DISABLED)
		return "DISABLED";
	else
		return "INVALID";
}

#ifdef CONFIG_WFD
static bool wfd_st_match_rule(_adapter *adapter, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port)
{
	struct wifi_display_info *wfdinfo = &adapter->wfd_info;

	if (ntohs(*((__be16 *)local_port)) == wfdinfo->rtsp_ctrlport ||
	    ntohs(*((__be16 *)local_port)) == wfdinfo->tdls_rtsp_ctrlport ||
	    ntohs(*((__be16 *)remote_port)) == wfdinfo->peer_rtsp_ctrlport)
		return true;
	return false;
}

static struct st_register wfd_st_reg = {
	.s_proto = 0x06,
	.rule = wfd_st_match_rule,
};
#endif /* CONFIG_WFD */

inline void rtw_wfd_st_switch(struct sta_info *sta, bool on)
{
#ifdef CONFIG_WFD
	if (on)
		rtw_st_ctl_register(&sta->st_ctl, SESSION_TRACKER_REG_ID_WFD, &wfd_st_reg);
	else
		rtw_st_ctl_unregister(&sta->st_ctl, SESSION_TRACKER_REG_ID_WFD);
#endif
}
