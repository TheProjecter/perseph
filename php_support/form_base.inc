<?php
/* ***** BEGIN LICENSE BLOCK *****
 * Version: LGPL 3.0
 * This file is part of Persephone's output and/or part of Persephone.
 *
 * This file is an exception to the main Persephone license in that
 * this file may be redistributed under the terms of the GNU
 * Lesser General Public License, version 3.
 * 
 * Contributors:
 *		edA-qa mort-ora-y <edA-qa@disemia.com>
 * ***** END LICENSE BLOCK ***** */

require_once dirname(__FILE__).'/dbsource.inc';
require_once dirname(__FILE__).'/base.inc';

abstract class DBS_FormBase {

	const ACTION_NONE = 0;
	const ACTION_SAVE = 1;
	const ACTION_DELETE = 2;
	const ACTION_ADD = 3;
	
	/**
	 * Performs form validation.
	 *
	 * @return [out] true if everything is okay, false otherwise
	 */
	abstract public function validate();
	
	/**
	 * Extracts data from the form into the entity provided.
	 */
	abstract public function extract( $entity );
	
	/**
	 * Take the values from the entity and put them in the form
	 */
	abstract public function inject( $entity );
	
	/**
	 * Obtains what action was called on this form. Note that
	 * ACTION_NONE does not necessarily imply that hasAction is
	 * false.
	 */
	abstract public function getAction();
	
	/** 
	 * This can be used to determine whether the form has been submitted yet,
	 * or this will be the first instance of the form (such as on first page access).
	 *
	 * This is a static since you will likely need to know this before creating
	 * the form object.
	 *
	 * @return [out] true indicates that some action  has been performed with the
	 *		form, false indicates no action has yet been performed.
	 */
	abstract public function hasAction();
	
	/**
	 * Produces the HTML rendering of the form
	 */
	abstract public function toHTML();
}

require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/Renderer/Default.php';	

/**
 * TODO: This does not handle the situation where the user can edit keys
 * and save the same record. The Entity framework can handle this, but
 * here it would require passing the original keys in the form.
 */
abstract class DBS_FormBase_QuickForm extends DBS_FormBase {
	
	protected $form;	//<HTML_QuickForm>
	protected $isNew = false; //<Bool>
	protected $createFrom;	//<Entity> created from this entity
	
	public $ENTITY = null;	//the class of the entity involved 
	
	public function initFromRequest() {
		$this->_setup();
		$this->isNew = $this->isNew();
	}
	
	public function initCreate( $from = null ) {
		$this->_setup();
		$this->createFrom = $from;
		$this->isNew = $from === null || $from->isNew();
	}
	
	abstract protected function _setup();
	
	public function validate() {
		return $this->form->validate();
	}
	
	const T_SUBMITROW = 'submitrow';
	const T_ACTION_SAVE = 'save';
	const T_ACTION_DELETE = 'delete';
	const T_ACTION_ADD = 'add';
	const T_ACTIONMARKER = '_dbs_actionmarker';	//TODO: unique name per form, to distinguish in hasAction
	
	public function getAction() {
		//see note in hasAction about exportValue
		$sr = isset( $_REQUEST[self::T_SUBMITROW] ) ? $_REQUEST[self::T_SUBMITROW] : null;
		if( $sr === null )
			return DBS_FormBase::ACTION_NONE;
		$act = key( $sr );
		switch( $act ) {
			case self::T_ACTION_SAVE:
				return DBS_FormBase::ACTION_SAVE;
			case self::T_ACTION_DELETE:
				return DBS_FormBase::ACTION_DELETE;
			case self::T_ACTION_ADD:
				return DBS_FormBase::ACTION_ADD;
			default:
				return DBS_FormBase::ACTION_NONE;
		}
	}
		
	public function hasAction() {
		//don't use exportValue since it requires the actions to have already been added
		//in the form, and stick this directly in the template
		return array_get_default( self::T_ACTIONMARKER, $_REQUEST, '0' ) == '1';
	}
	
	static public function isNew() {
		return array_get_default( self::T_MARKNEW, $_REQUEST, '0' ) == '1';
	}
	
	const T_MARKNEW = '_dbsfq_new';
	
	public function execute() {
		if( !$this->hasAction() ) {
			if( $this->createFrom !== null )
				$this->inject( $this->createFrom );
		}
			
		$showForm = true;
		if( $this->validate() ) {
			if( $this->isNew ) {
				$rule = call_user_func( "{$this->ENTITY}::createWithNothing" );
			} else {
				$rule = call_user_func( "{$this->ENTITY}::withNothing" );
				$this->extractKeys( $rule );
				$rule->find();
			}
			
			try {
				$this->extract( $rule );
				if( $this->getAction() == DBS_FormBase::ACTION_SAVE ) {
					$rule->save();
					$this->inject( $rule, true );	//capture any logic/new values from entity
					print( "<p class='success'>Saved.</p>" );
				} else if( $this->getAction() == DBS_FormBase::ACTION_ADD ) {
					$rule->add();
					$this->isNew = false;
					$this->inject( $rule, true );	//capture any logic/new values from entity
					print( "<p class='success'>Added.</p>" );
				} else if( $this->getAction() == DBS_FormBase::ACTION_DELETE ) {
					$rule->delete();
					$showForm = false;
					print( "<p class='success'>Deleted.</p>" );
				}
			} catch( DBS_SetFieldException $ex ) {
				//just do something quick and dirty for now, TODO: combine with HTMLQuickForm to report validation errors
				print( "<p class='error'>{$ex->getMessage()}</p>" );
			}
			//TODO: Other exceptions...
		}
		
		if( $showForm ) {
			$this->addActions();
			echo $this->toHTML();
		}
	}
	
	public function toHTML() {
		
		//////////////////////////////////////////////////////////////////////////////////////
		//The other template engines, being used with QuickForm suffer from a significant
		//lack of documentation, and several bugs, this one is also too limited, but it'll be
		//fine for now.
		$render = new HTML_QuickForm_Renderer_Default();
		$render->setFormTemplate( "
			<form{attributes}>
				<input type='hidden' name='" . self::T_ACTIONMARKER . "' value='1'/>
				<input type='hidden' name='" . self::T_MARKNEW . "' value='" . ($this->isNew ? '1' : '0') . "'/>
				<table class='data_bound'><tr><td><fieldset class='data table'><table class='data'>
					{content}
				</table></fieldset></tr></td></table>
			</form>
			");
		$render->setElementTemplate( "
		<tr>
			<td class='name'>
				<!-- BEGIN required --><span style=\"color: #ff0000\">*</span><!-- END required -->
				{label}
			</td>
			<td class='value'>
				{element}
				<!-- BEGIN error --><br/><span style=\"color: #ff0000\">{error}</span><!-- END error -->
			</td>
		</tr>
			");
		$render->setHeaderTemplate( "
			<tr class='header'>
				<td colspan='2'>{header}</td>
			</tr>
			");		
		$render->setGroupTemplate( "
			<tr class='submit'>
				<td colspan='2' class='submit'>
					{content}
				</td>
			</tr>
			", "submitrow"
			);
	
		$this->form->accept( $render );
		return $render->toHtml();
	}
	
}

function _dbs_formin_Integer( $value ) { return $value === null ? '' : '' . $value; }
function _dbs_formin_String( $value ) {	return $value === null ? '' : $value; }
function _dbs_formin_Text( $value ) {	return $value === null ? '' : $value; }
function _dbs_formin_Decimal( $value ) { return $value === null ? '' : '' . $value; }
function _dbs_formin_Float( $value ) { return $value === null ? '' : '' . $value; }
function _dbs_formin_Bool( $value ) {	return $value === null ? '' : $value ? '1' : '0'; }
function _dbs_formin_DateTime( $value ) {	return $value === null ? '' : date( 'Y-m-d H:i:s', $value ); }
function _dbs_formin_Date( $value ) {	return $value === null ? '' : date( 'Y-m-d', $value ); }
function _dbs_formin_Time( $value ) {	return $value === null ? '' : _dbs_form_encode_time($value ); }

function _dbs_formout_Integer( $value ) { return $value === '' ? null : intval( $value ); }
function _dbs_formout_String( $value ) {	return $value; }
function _dbs_formout_Text( $value ) {	return $value; }
function _dbs_formout_Decimal( $value ) { return $value === '' ? null : floatval( $value ); }
function _dbs_formout_Float( $value ) { return $value === '' ? null : floatval( $value ); }
function _dbs_formout_Bool( $value ) {	return $value === '' ? null : '1' === $value; }
function _dbs_formout_DateTime( $value ) { return $value === '' ? null : strtotime( $value ); }
function _dbs_formout_Date( $value ) { return $value === '' ? null : strtotime( $value ); }
function _dbs_formout_Time( $value ) { return $value === '' ? null : _dbs_form_decode_time( $value ); }

function _dbs_form_decode_time( $value ) {
	$parts = explode( ':', $value );
	if( count( $parts ) != 3 )	
		return null;
		
	return $parts[0] * 3600 + $parts[1] * 60 + $parts[2];
}

function _dbs_form_encode_time( $value ) {
	return sprintf( "%d:%02d:%02d", floor($value/3600), floor($value/60) % 60, $value % 60 );
}

function _dbs_form_loadentityselect( $results, $key, $text ) {
	$ret = array();
	foreach( $results as $result )
		$ret[$result->$key] = $result->$text;
	return $ret;
}

/**
 * Does a  very standard form setup, either editing an entity or creating a
 * new one with defaults.
 *
 * @param formclass [in] which form class to use, implies the entity
 * @param entitykey [in] which findWith function to use (such as "ID") and
 *		also specifies the request parameter used to indicate this parameter, 
 *		if not specified in request then a new item is assumed
 * @param defaultsForNew [in] if the key is not in the request, then these
 *		defaults are set in the form.  Form:
 *			memberName -> value
 *		If value is null the item will not be set (this is for convenience)
 */
function dbs_standard_form( $form, $entitykey, $defaultsForNew = null ) {
	if( !$form->hasAction() ) {
		$key = array_get_default( $entitykey, $_REQUEST, null );	//TODO: unlike the get_request_def this doesn't handle encoding correctly!!!
		$entityname = $form->ENTITY;
		//for PHP 5.3 (and can also replace call_user_func)
		//$entityname = $formclass::ENTITY
		if( $key !== null ) 
			$rule = call_user_func( "$entityname::findWith$entitykey", $key );
		else if( $defaultsForNew !== null ) {
			$rule = call_user_func( "$entityname::createWithNothing" );
			foreach( $defaultsForNew as $member => $value ) {
				if( $value !== null )
					$rule->$member = $value;
			}
		} else
			$rule = null;
			
		$form->initCreate( $rule );
	} else {
		$form->initFromRequest( );
	}
	return $form;
}

?>